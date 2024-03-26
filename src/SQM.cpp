
#include <boost/system/system_error.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/format.hpp>
#include "SQM.h"
#include "GLog.h"

using namespace boost::system;
using namespace boost::filesystem;
using namespace boost::asio;
using namespace boost::posix_time;

SQMUnitVec SQM::units_;
boost::condition_variable SQM::cvFound_;

SQM::SQM(const char* ip, const char* dirName)
    : portDev_(10001) {
    if (dirName) dirRoot_ = dirName;
    ipDev_ = ip;
    cntRsp_= 0;
    fpLog_ = NULL;
}

SQM::~SQM() {
    interrupt_thread(thrdCycle_);
    tcpClient_.reset();
    if (fpLog_) {
        fclose(fpLog_);
        fpLog_ = NULL;
    }
    _gLog.Write("SQM: stopped");
}

void SQM::handle_found(const boost::system::error_code& ec, const int bytes) {
    if (!ec && bytes >= 30) cvFound_.notify_one();
}

int SQM::Find() {
    try {
        boost::chrono::seconds toWait(1);
        units_.clear();
        // 建立UDP套接口
        BoostAsioKeep keep;
        ip::udp::socket sock(keep.GetIOService());
        sock.open(ip::udp::v4());
        sock.set_option(socket_base::broadcast(true));
        // 构建并发送信息
        ip::udp::endpoint broadcast(ip::address::from_string("255.255.255.255"), 30718);
        unsigned char query[] = {0, 0, 0, 0xF6};
        sock.send_to(buffer(query), broadcast);
        // 监测反馈信息
        boost::mutex mtx;
        MtxLck lck(mtx);
        unsigned char rcvd[100];
        char mac[20];
        ip::udp::endpoint remote;

        while (1) {
            sock.async_receive_from(buffer(rcvd), remote,
                bind(&SQM::handle_found, placeholders::error, placeholders::bytes_transferred));
            if (cvFound_.wait_for(lck, toWait) == boost::cv_status::timeout) break;

            if (rcvd[0] == 0 && rcvd[1] == 0 && rcvd[2] == 0 && rcvd[3] == 0xF7) {
                SQMUnit unit;
                int n(0);
                for (int i = 24; i <= 29; ++i) n += sprintf(mac + n, "%02X:", rcvd[i]);
                mac[n - 1] = 0;
                unit.ip = remote.address().to_string();
                unit.mac= mac;
                units_.push_back(unit);
                _gLog.Write("SQM: found [%s,  %s]", unit.ip.c_str(), unit.mac.c_str());
            }
        }
        // 释放套接口
        sock.close();
        // 返回找到的设备数量
        return units_.size();
    }
    catch(system_error& ex) {
		_gLog.Write(LOG_WARN, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
    }

    return 0;
}

bool SQM::GetAddress(int index, string& ip, string& mac) {
    if (index >= 0 && index < units_.size()) {
        ip = units_[index].ip;
        mac= units_[index].mac;
        return true;
    }
    return false;
}

bool SQM::Start(int cycle) {
    thrdCycle_.reset(new boost::thread(boost::bind(&SQM::run, this, cycle)));

    return true;
}

bool SQM::IsConnected() {
    return (tcpClient_.unique() && tcpClient_->IsOpen());
}

void SQM::run(int cycle) {
    boost::chrono::seconds toWait(cycle);
    char query[] = "rx";
    int cntQry(0);

    info_.state = 0;
    while (1) {
        if (!tcpClient_.unique()) {// 尝试连接
            tcpClient_ = TcpClient::Create();
            const TcpClient::CBSlot& slot = boost::bind(&SQM::handle_receive, this, boost::placeholders::_1, boost::placeholders::_2);
            tcpClient_->RegisterRead(slot);
            if (!tcpClient_->Connect(ipDev_.c_str(), portDev_)) {
                tcpClient_.reset();
                info_.state = SQM_FAIL_CONNECT;
                _gLog.Write(LOG_FAULT, "[%s:%d], failed to connect SQM[%s:%u]",
                    __FILE__, __LINE__, ipDev_.c_str(), portDev_);
            }
            else {
                info_.state = SQM_SUCCESS;
                info_.utc   = to_iso_extended_string(second_clock::universal_time());
                info_.mpsas = 0.0;
                oldDay_     = 0;
                cntQry = cntRsp_ = 0;

                _gLog.Write("SQM: starts working...");
            }
        }
        if (tcpClient_.unique()) {
            if ((cntQry - cntRsp_) > 5) {
                info_.state = SQM_NO_DATA;
                _gLog.Write(LOG_WARN, "SQM: long time no data response");
            }

            if (info_.state != SQM_SUCCESS) {
                tcpClient_->Close();
                tcpClient_.reset();
            }
            else {// 发送查询指令
                ++cntQry;
                tcpClient_->Write(query, sizeof(query));
            }
        }
        boost::this_thread::sleep_for(toWait);
    }
}

void SQM::handle_receive(TcpClient* client, const boost::system::error_code ec) {
    static char buff[64];
    static char mpsas[8];
    if (!ec && client->Read(buff, 57) == 57 && buff[0] == 'r') {
        ptime tmNow = second_clock::universal_time();
        memcpy(mpsas, buff + 2, 6);
        buff[57] = 0;
        mpsas[6] = 0;
        info_.utc   = to_iso_extended_string(tmNow);
        info_.mpsas = float(atof(mpsas));

        ++cntRsp_;
#ifdef NDEBUG
        _gLog.Write("SQM: %s => %6.2f", buff, info_.mpsas);
#endif

        // 写入文件
        ptime::date_type today = tmNow.date();
        if (open_file(today.year(), today.month().as_number(), today.day())) {
            fprintf(fpLog_, "%s  %6.2f\n", info_.utc.c_str(), info_.mpsas);
            fflush(fpLog_);
        }
    }
    else {
        info_.state = SQM_CLOSED;
        _gLog.Write(LOG_WARN, "SQM: remote closed");
    }
}

bool SQM::open_file(int year, int month, int day) {
    if (oldDay_ != day) {
        if (fpLog_) {
            fclose(fpLog_);
            fpLog_ = NULL;
        }

        try {
            // 创建目录
            path pathName(dirRoot_);
            pathName /= (boost::format("SQM").str());
            if (!exists(pathName)) {
                create_directory(pathName);
                permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
            }
            pathName /= (boost::format("Y%d") % year).str();
            if (!exists(pathName)) {
                create_directory(pathName);
                permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
            }
            // 打开文件
            boost::format fmtFile("SQM_%d%02d%02d.log");
            pathName /= (fmtFile % year % month % day).str();
            _gLog.Write("SQM File = %s", pathName.c_str());
            fpLog_ = fopen(pathName.c_str(), "a+");

            // 保存日期
            oldDay_ = day;
        }
        catch(filesystem_error& ex) {
            _gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
            return false;
        }
    }

    return fpLog_ != NULL;
}
