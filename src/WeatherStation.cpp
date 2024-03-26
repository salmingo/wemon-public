
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "WeatherStation.h"
#include "GLog.h"

using namespace boost::posix_time;
using namespace boost::placeholders;
using namespace boost::filesystem;

// 气象站
#define WEA_FUNC    0x03    // 功能码: 查询
#define WEA_THP     0x66    // 地址: 温度、湿度、气压
#define WEA_WIND    0xC8    // 地址: 风速、风向
#define WEA_RAIN    0xCA    // 地址: 雨量

// MODBUS协议格式
// 指令
// 功能码: 0x03--查询; 0x06--修改
//                           地址  功能码   寄存器地址    寄存器数量   校--验--码
unsigned char qryTHP[]    = {0x66, 0x03, 0x00, 0x00, 0x00, 0x03, 0x0D, 0xDC};
unsigned char qryWind[]   = {0xC8, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD5, 0x92};
// unsigned char qryRainy[]  = {0xCA, 0x03, 0x00, 0x00, 0x00, 0x02, 0xD4, 0x70};
//
// 反馈
//                            地址  功能码  长度    数---值    校--验--码
// unsigned char rspTemp[] = {0x66, 0x03, 0x02, 0x09, 0xC4, 0x8A, 0x4F};
// 以上

// 独立雨水
// 指令
unsigned char qryRainy[]  = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
// 反馈
// 有雨 : 0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84
// 无雨 : 0x01, 0x03, 0x02, 0x00, 0x00, 0xB8, 0x44
// 以上

WeatherStation::WeatherStation(const char*portwea, const char* portrain, const char* dirName) {
    if (dirName) dirRoot_ = dirName;
    portWea_ = portwea;
	portRain_= portrain;
}

WeatherStation::~WeatherStation() {
    interrupt_thread(thrdQuery_);
    if (weaPtr_.unique()) {
        weaPtr_->Close();
        weaPtr_.reset();
    }
	if (rainPtr_.unique()) {
		rainPtr_->Close();
		rainPtr_.reset();
	}
    if (fpLog_) fclose(fpLog_);
    _gLog.Write("Weather Station: stopped");
}

bool WeatherStation::Start(int cycle) {
    thrdQuery_.reset(new boost::thread(boost::bind(&WeatherStation::run, this, cycle)));

    return true;
}

bool WeatherStation::IsRun() {
    return weaPtr_.use_count();
}

void WeatherStation::run(int cycle) {
   	int cntErrWea(0), cntErrRain(0), noReadWea(0), noReadRain(0), toWait;
    ptime tmBeg = second_clock::universal_time();

    while (1) {
    	if (!weaPtr_.unique()) {// 连接串口
            const SerialComm::CBSlot& slot = boost::bind(&WeatherStation::handle_receive_weather, this, _1, _2, _3);
            weaPtr_ = SerialComm::Create();
            if (!weaPtr_->Open(portWea_.c_str())) {
                weaPtr_.reset();
                info_.state = WEA_FAIL_CONNECT;
                _gLog.Write(LOG_FAULT, "[%s:%d], failed to connect Weather Station[%s]",
                    __FILE__, __LINE__, portWea_.c_str());
            }
            else {
				weaPtr_->SetReadLength(7);
	            weaPtr_->RegisterRead(slot);
                info_.state = WEA_SUCCESS;
                info_.utc   = to_iso_extended_string(second_clock::universal_time());
                oldDay_   = 0;
                noReadWea = 0;
                _gLog.Write("Weather Station: connected");
            }
        }

		if (!rainPtr_.unique()) {
			const SerialComm::CBSlot& slot = boost::bind(&WeatherStation::handle_receive_rain, this, _1, _2, _3);
			rainPtr_ = SerialComm::Create();
			if (!rainPtr_->Open(portRain_.c_str(), 4800)) {
				rainPtr_.reset();
                _gLog.Write(LOG_FAULT, "[%s:%d], failed to connect Rain Monitor[%s]",
                    __FILE__, __LINE__, portRain_.c_str());
			}
			else {
				rainPtr_->SetReadLength(7);
				rainPtr_->RegisterRead(slot);
				info_.rainFall = 0;
				noReadRain = 0;
				_gLog.Write("Rain Monitor: connected");
			}
		}

        if (weaPtr_.unique()) {// 查询气象信息
            cntErrWea = 0;

            qryType_ = WEA_THP;
            weaPtr_->Write((const char*) qryTHP,    sizeof(qryTHP));
            wait_response(cntErrWea);

            qryType_ = WEA_WIND;
            weaPtr_->Write((const char*) qryWind,   sizeof(qryWind));
            wait_response(cntErrWea);

            info_.state = cntErrWea ? WEA_NO_DATA : WEA_SUCCESS;
            if (!cntErrWea) {
                ptime::date_type today = tmBeg.date();
                info_.utc   = to_iso_extended_string(tmBeg);
                if (open_file(today.year(), today.month().as_number(), today.day())) {
                	fprintf(fpLog_, "%s %5.1f %5.1f %6.1f %4.1f %3d %10u\n", info_.utc.c_str(),
                    	    info_.temperature, info_.humidity, info_.pressure,
                    	    info_.windSpeed, info_.windOrient,
                    	    info_.rainFall);
                	fflush(fpLog_);
	            }

                noReadWea = 0;
	        }
            else if (++noReadWea >= 3) {
                weaPtr_->Close();
                weaPtr_.reset();
            }
	    }

		if (rainPtr_.unique()) {// 监测降雨信号
			cntErrRain = 0;
			rainPtr_->Write((const char*) qryRainy, sizeof(qryRainy));
			wait_response(cntErrRain);

			if (!cntErrRain) noReadRain = 0;
			else if (++noReadRain >= 3) {
				rainPtr_->Close();
				rainPtr_.reset();
			}
		}

	    // 延时等待
        toWait = cycle - (second_clock::universal_time() - tmBeg).total_seconds();
        tmBeg += seconds(cycle);
        if (toWait > 0) boost::this_thread::sleep_for(boost::chrono::seconds(toWait));
    }
}

void WeatherStation::wait_response(int &cntErr) {
    boost::mutex mtx;
    MtxLck lck(mtx);
    boost::chrono::seconds toWait(5); // 等待串口反馈
    boost::chrono::seconds delay(1); // 连续指令间隔

    if (cvGet_.wait_for(lck, toWait) == boost::cv_status::no_timeout) {
        boost::this_thread::sleep_for(delay);
    }
    else {
        ++cntErr;
    }
}

void WeatherStation::handle_receive_weather(SerialComm* comm, int ec, size_t bytes) {
	if (ec) info_.state = ec;
	else {
		static unsigned char flag[] = {0x03};
		size_t len = sizeof(flag);
		static unsigned char buff[20];
		int Npck, datalen;
		int pos = weaPtr_->Lookup((const char*) flag, len);
		if (pos >= 1) {
			weaPtr_->Read((char*) buff, 3, pos - 1, false); // 帧头
			datalen = buff[0] == WEA_THP ? 6 : 4;
			if (buff[2] != datalen) {// 容错:
				weaPtr_->Read((char*) buff, 2, pos - 1);
			}
			else if (bytes >= (Npck = 5 + datalen)) {// 数据和校验码
				weaPtr_->Read((char*) buff, Npck, pos - 1);
				if (buff[0] == WEA_THP) {
                    info_.temperature = ((buff[3] << 8) + buff[4]) * 0.01;
					info_.humidity    = ((buff[5] << 8) + buff[6]) * 0.01;
                    info_.pressure    = ((buff[7] << 8) + buff[8]) * 0.1;
				}
				else if (buff[0] == WEA_WIND) {
                    info_.windSpeed   = ((buff[3] << 8) + buff[4]) * 0.01;
                    info_.windOrient  = ((buff[5] << 8) + buff[6]);
				}

				cvGet_.notify_one();
			}
		}
		else if (pos == 0) {// 容错
			weaPtr_->Read((char*) buff, 1);
		}
	}
}

void WeatherStation::handle_receive_rain(SerialComm* comm, int ec, size_t bytes) {
	if (ec) info_.state = ec;
	else {
		static unsigned char flag[] = {0x01, 0x03, 0x02};
		size_t len = sizeof(flag);
		static unsigned char buff[10];
		int Npck(7);
		int pos = rainPtr_->Lookup((const char*) flag, len);

		if (pos >= 0 && bytes >= (Npck + pos)) {
			rainPtr_->Read((char*) buff, Npck, pos);
			if (buff[4] == 0x01) info_.rainFall = 1;
			else if (buff[4] == 0x00) info_.rainFall  = 0;

			cvGet_.notify_one();
		}
	}
}

bool WeatherStation::open_file(int year, int month, int day) {
    if (oldDay_ != day) {
        if (fpLog_) fclose(fpLog_);
        try {
            // 创建目录
            path pathName(dirRoot_);
            pathName /= "Weather";
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
            boost::format fmtFile("Weather_%d%02d%02d.log");
            pathName /= (fmtFile % year % month % day).str();
            _gLog.Write("Weather File = %s", pathName.c_str());
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

unsigned short WeatherStation::modbus_crc16(unsigned char* data, unsigned int len) {
	unsigned int i, j;
	unsigned short CRC16, tmp;

	CRC16 = 0xFFFF;
	for (i = 0; i < len; ++i) {
		CRC16 ^= data[i];
		for (j = 0; j < 8; ++j) {
			tmp = (unsigned short) (CRC16 & 0x0001);
			CRC16 >>= 1;
			if (tmp == 1) CRC16 ^= 0xA001;
		}
	}
	return CRC16;
}

void WeatherStation::value2code(unsigned short crc, char* code) {
    code[0] = (char) (crc & 0xFF);
    code[1] = (char) ((crc >> 8) & 0xFF);
}
