/**
 * last-update:
 * @date 2023-11-04
 * @note
 * - 更新AsioUP/AsioTCP
 * - 屏蔽SQM
 * - 修改云量相机自动调焦两个判据
 */

#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include "GLog.h"
#include "ADefine.h"
#include "EnvMonitor.h"
#include "ATimeSpace.h"
#include "AstroDeviceDef.h"
#include "ProtocolPDXP.h"
#include "ProtoFocus.h"

using namespace boost::posix_time;
using namespace boost::filesystem;
using namespace boost::placeholders;
using namespace AstroUtil;

EnvMonitor::EnvMonitor(const Parameter* param) {
	param_ = param;
}

EnvMonitor::~EnvMonitor() {
}

bool EnvMonitor::Start() {
	try {// 尝试创建目录结构
		// 云图原始数据根目录
		path pathName(param_->dirRawImage);
		if (!exists(pathName)) {
			create_directory(pathName);
			permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
		}

		// 测量数据根目录
		pathName = param_->sampleDir;
		if (!exists(pathName)) {
			create_directory(pathName);
			permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
		}
	}
	catch(filesystem_error& ex) {
		_gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
		return false;
	}

	odt_ = TypeObservationDuration::ODT_MIN;
	thrdTwilight_.reset(new boost::thread(boost::bind(&EnvMonitor::monitor_twilight, this)));
	if (param_->minDiskFree > 0) thrdDisk_.reset(new boost::thread(boost::bind(&EnvMonitor::thread_diskfree, this)));
	// 启动网络上传流程
	if (param_->enablePDXP) thrdPDXP_.reset(new boost::thread(boost::bind(&EnvMonitor::thread_pdxp, this)));

	weaStatPtr_ = WeatherStation::Create(param_->portWeaStation.c_str(), param_->portRain.c_str(), param_->sampleDir.c_str());
	weaStatPtr_->Start(param_->sampleCycle);

	readCloudagePtr_ = ReadCloudage::Create();
	readCloudagePtr_->Start(param_);

	// 网络服务
	udpCmd_ = UdpSession::Create();
	if (!udpCmd_->Open(param_->portCommand)) {
		const UdpSession::CBSlot& slot = boost::bind(&EnvMonitor::udp_receive_command, this, _1, _2);
		udpCmd_->RegisterReceive(slot);
		_gLog.Write(LOG_WARN, "failed to create UDP server on [%d] for command", param_->portCommand);
	}

	return true;
}

void EnvMonitor::Stop() {
	udpCmd_.reset();

	interrupt_thread(thrdDisk_);
	interrupt_thread(thrdTwilight_);
	interrupt_thread(thrdPDXP_);

	if (camCloudPtr_.unique()) {
		camCloudPtr_->Stop();
		camCloudPtr_.reset();
	}

	sqmPtr_.reset();
	readCloudagePtr_.reset();
	weaStatPtr_.reset();
}

/*========================== 线程 ==========================*/
void EnvMonitor::monitor_twilight() {
	double sunrise, sunset;// 小时, 本地时

	while (1) {
		{// 计算晨昏蒙影
			ptime utc = second_clock::universal_time();
			ptime::date_type today = utc.date();
			ATimeSpace ats;
			ats.SetSite(param_->siteLon, param_->siteLat, param_->siteAlt, -timezone/3600);
			ats.SetUTC(today.year(), today.month().as_number(), today.day(), utc.time_of_day().total_seconds() / AU_DAYSEC);
			if (ats.TimeOfSunAlt(sunrise, sunset, param_->sunEleMax)) {
				sunrise = 24;
				sunset  = 0;
			}
			char risestr[20], setstr[20];
			ats.HourDbl2Str(sunrise, risestr);
			ats.HourDbl2Str(sunset,  setstr);
			_gLog.Write("Observation Duration: From = %s,  To = %s", setstr, risestr);
		}
		{// 等待至昏影时
			odt_ = TypeObservationDuration::ODT_DAYTIME;
			double hours = second_clock::local_time().time_of_day().total_seconds() / 3600.0;
			if (hours > sunrise && hours < sunset) {
				int seconds = int((sunset - hours) * 3600 + 1.5);
				boost::this_thread::sleep_for(boost::chrono::seconds(seconds));
			}
			odt_ = TypeObservationDuration::ODT_NIGHT;
		}
		{// 1: 启动观测流程
			camCloudPtr_ = CloudCamera::Create(param_);
			camCloudPtr_->Start();

			sqmPtr_ = SQM::Create(param_->addrSQM.c_str(), param_->sampleDir.c_str());
			sqmPtr_->Start(param_->sampleCycle);
		}
		{// 观测至晨光始
			double hours = second_clock::local_time().time_of_day().total_seconds() / 3600.0;
			if ((hours = sunrise - hours) < 0) hours += 24;
			int seconds = int(hours * 3600 + 1.5);
			boost::this_thread::sleep_for(boost::chrono::seconds(seconds));
		}
		{// 停止观测流程
			_gLog.Write("Cloud Camera stopped for entering into day time");
			_gLog.Write("SQM stopped for entering into day time");

			camCloudPtr_.reset();
			sqmPtr_.reset();
		}
	}
}

void EnvMonitor::thread_diskfree() {
	while (1) {
		ptime tmNow = second_clock::local_time();
		ptime tmNoon(tmNow.date(), hours(12));
		int toWait = (tmNoon - tmNow).total_seconds();
		if (toWait <= 0) toWait += 86400;
		boost::this_thread::sleep_for(boost::chrono::seconds(toWait));

		try {// 检查磁盘可用空间
			path pathDir(param_->dirRawImage);
			space_info si = space(pathDir);
			if ((si.available >> 30) > param_->minDiskFree) continue;
			_gLog.Write(LOG_WARN, "free disk capacity [%d] GB is less than threshold...starts erasing the oldest data",
				si.available >> 30);

			// 查找待删除目录
			typedef std::vector<string> stringvec;
			stringvec dirList;
			string subName;
			for (directory_iterator it = directory_iterator(pathDir); it != directory_iterator(); ++it) {
				if (is_directory(it->path())) {
					subName = it->path().filename().string();
					if (subName.find(param_->prefixName) == 0) dirList.push_back(it->path().string());
				}
			}
			std::stable_sort(dirList.begin(), dirList.end(), [](const string& ls, const string& rs) {
				return ls < rs;
			});

			// 删除历史数据
			for (stringvec::iterator it = dirList.begin(); it != dirList.end(); ++it) {
				remove_all(path(*it));

				si = space(pathDir);
				if ((si.available >> 30) > param_->minDiskFree) break;
			}
			_gLog.Write("disk erasing complete, free capacity is %d GB", si.available >> 30);
		}
		catch(filesystem_error& ex) {
			_gLog.Write(LOG_FAULT, "[%s : %s], %s", __FILE__, __FUNCTION__, ex.what());
		}
	}
}

void EnvMonitor::thread_pdxp() {
	boost::chrono::seconds toWait(param_->sampleCycle <= 10 ? 10 : param_->sampleCycle);
	uint32_t pno(0);
	UdpPtr udp = UdpSession::Create();
	udp->Open();

	while (1) {
		boost::this_thread::sleep_for(toWait);
		upload_pdxp(udp,  pno,  param_->addrPDXP.c_str(),  param_->portPDXP);
        // save_json();   ///< 保存事后气象数据
		++pno;
	}
}

void EnvMonitor::save_json() {
    // 气象信息写入wea文件

    const InfoCloudage* nfCloudage = readCloudagePtr_->GetInfo();
    string pathName = log_filepath(nfCloudage);
    if (pathName.empty()) return;

    boost::property_tree::ptree pt;
    std::string Mtime = nfCloudage->utc;
    std::replace(Mtime.begin(), Mtime.end(), 'T', ' ');
    std::replace(Mtime.begin(), Mtime.end(), '-', ' ');
    std::replace(Mtime.begin(), Mtime.end(), ':', ' ');
    std::replace(Mtime.begin(), Mtime.end(), '.', ' ');

    pt.add("SiteID", 108);
//    pt.add("DeviceID", 5606);
    pt.add("DeviceID", 5606);
    pt.add("MTIME", Mtime);

    // 生成wea 插入的时间（无效值）
    boost::property_tree::ptree& ptWeather = pt.add("Weather", ""); // 填充气象信息到wea文件
    // 填写weather信息到wea文件
    ptWeather.add("State", 1);
    ptWeather.add("WUTC", Mtime);
    ptWeather.add("T2", -99.9);         ///< 温度
    ptWeather.add("Q2", -99.9);         ///< 湿度>
    ptWeather.add("PS", -99.9);         ///< 气压>
    ptWeather.add("Td", -99.9);         ///< 露点温度 计算露点温度 RH = 100 - 5 * (T-Td)
    ptWeather.add("SPD", -99.9);        ///< 风速>
    ptWeather.add("DIR", -99.9);        ///< 风向>
    ptWeather.add("isRain", -99.9);     ///< 降雨>
    ptWeather.add("TR", -99.9);         ///< 每小时降雨量
    ptWeather.add("TF", -99.9);         ///< 总云量，有云区域占全天区的百分比
    ptWeather.add("GEOTF", -99.9);      ///< 同步轨道区域总云量

    if (weaStatPtr_.unique() && weaStatPtr_->IsRun()) {
        const InfoWeather* nfWea = weaStatPtr_->GetInfo();
        if (nfWea->state != WEA_NO_DATA) {
            ptWeather.put("State", 0);
            std::string wUtc = nfWea->utc;
            std::replace(wUtc.begin(), wUtc.end(), 'T', ' ');
            std::replace(wUtc.begin(), wUtc.end(), '-', ' ');
            std::replace(wUtc.begin(), wUtc.end(), ':', ' ');
            ptWeather.put("WUTC", wUtc);
            ptWeather.put("T2", nfWea->temperature);
            ptWeather.put("Q2", nfWea->humidity);
            ptWeather.put("PS", nfWea->pressure);
            float td = nfWea->temperature - ((100 - nfWea->humidity) / 5); // 计算露点温度 RH = 100 - 5 * (T-Td)
            ptWeather.put("Td", td);
            ptWeather.put("SPD", nfWea->windSpeed);
            ptWeather.put("DIR", nfWea->windOrient);
            ptWeather.put("isRain", nfWea->rainFall);
            ptWeather.put("TR", -99.9);  //每小时降雨量，咱无法测量
            ptWeather.put("TF", -99.9);  //总云量，有云区域占全天区的百分比
            ptWeather.put("GEOTF", -99.9); //同步轨道区域总云量
        }
    }

    // 填充夜天光
    boost::property_tree::ptree& ptSQM = pt.add("SQM", ""); // 在wea文件写入夜天光
    ptSQM.add("State", 1);
    ptSQM.add("SQMUTC", Mtime);
    ptSQM.add("MPSAS", -99.9);
    if(sqmPtr_.unique()) {
        if (sqmPtr_->IsConnected()) {
            const InfoSQM* nfSQM = sqmPtr_->GetInfo();
            if (nfSQM->state != SQM_NO_DATA) {
                // 填写sqm 到 wea文件
                string sqmUtc = nfSQM->utc;
                std::replace(sqmUtc.begin(), sqmUtc.end(), '-', ' ');
                std::replace(sqmUtc.begin(), sqmUtc.end(), ':', ' ');
                std::replace(sqmUtc.begin(), sqmUtc.end(), 'T', ' ');

                ptSQM.put("State",    0);
                ptSQM.put("SQMUTC", sqmUtc);
                ptSQM.put("MPSAS",  nfSQM->mpsas);
            }
        }
    }

    // 填写 云量分布到wea文件
    boost::property_tree::ptree& ptCloudage = pt.add("Cloudage", ""); // 在wea文件写入云量信息
    int zone_count = nfCloudage->zones.size();
    ptCloudage.add("State", 1);
    ptCloudage.add("CLOUTC", Mtime);
    ptCloudage.add("Coordinate", 0);                    //0：地平坐标，1：GEO星下点经纬度坐标
    ptCloudage.add("PointCount", zone_count);           //云量分布指向总数目
    ptCloudage.add("Angle1Step", nfCloudage->azStep);   //地平坐标下的方位角步长
    ptCloudage.add("Angle2Step", nfCloudage->elStep);   //地平坐标下的俯仰角步长

    if (camCloudPtr_.unique() && camCloudPtr_->GetInfo()->state == WMC_SUCCESS) {
        int state = readCloudagePtr_->GetInfo()->state;
        if ((state == WMCA_NO_DATA ? 1 : (state == WMCA_TOO_OLD ? 2 : 0)) == 0) {
            // 统计全天云量
            const CloudAgeSet& caSet = nfCloudage->zones;
            int zone_count = caSet.size();
            int zone_greater_7(0);
            for (int i = 0; i < zone_count; ++i) {
                if (std::get<2>(caSet[i]) >= 7) ++zone_greater_7;
            }
            // 填写 云量分布到wea文件
            ptCloudage.put("State", 0);

            std::string cloudUtc = nfCloudage->utc;
            std::replace(cloudUtc.begin(), cloudUtc.end(), 'T', ' ');
            std::replace(cloudUtc.begin(), cloudUtc.end(), ':', ' ');
            std::replace(cloudUtc.begin(), cloudUtc.end(), '-', ' ');

            ptCloudage.put("CLOUTC", cloudUtc);
            ptCloudage.put("Coordinate", 0);                    //0：地平坐标，1：GEO星下点经纬度坐标
            ptCloudage.put("PointCount", zone_count);           //云量分布指向总数目
            ptCloudage.put("Angle1Step", nfCloudage->azStep);   //地平坐标下的方位角步长
            ptCloudage.put("Angle2Step", nfCloudage->elStep);   //地平坐标下的俯仰角步长
            boost::property_tree::ptree angle1;
            boost::property_tree::ptree angle2;
            boost::property_tree::ptree levels;
            for (int i = 0; i < zone_count; ++i) {
                boost::property_tree::ptree azis;
                azis.put("", std::get<0>(nfCloudage->zones[i]));
                angle1.push_back((std::make_pair("",azis)));

                boost::property_tree::ptree eles;
                eles.put("", std::get<1>(nfCloudage->zones[i]));
                angle2.push_back((std::make_pair("",eles)));

                boost::property_tree::ptree level;
                level.put("", std::get<2>(nfCloudage->zones[i]));
                levels.push_back((std::make_pair("",level)));
            }
            ptCloudage.put_child("Angle1", angle1);            //地平坐标下的方位角
            ptCloudage.put_child("Angle2", angle2);            //地平坐标下的俯仰角
            ptCloudage.put_child("Level", levels);             //对应天区的云量等级
        }
    }
    try {
        boost::property_tree::write_json(pathName, pt);
    }
    catch(boost::property_tree::json_parser_error& ex) {
        _gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
    }

}

void EnvMonitor::upload_pdxp(UdpPtr udp, uint32_t pno, const char* ip, int port) {
	ArrayChar bufWrite = make_shared_array<char>(UDP_PACK_SIZE); // UDP输出缓冲区
	int zone_max(72); // 每包数据中最大的云量天区数

	// 气象自适应信息
	PDXP_QXZSY qxzsy;	// 气象自适应信息
	qxzsy.pno = ++pno;
	int byteQXZSY = sizeof(PDXP_QXZSY);
	int bytePerCloudage = sizeof(PDXP_Cloudage);

	// 初始化云量分布状态
	if (camCloudPtr_.unique() && camCloudPtr_->GetInfo()->state != WMC_SUCCESS) {
		qxzsy.cloud_state = 1; // 设备工作异常
	}
	else {
		int state = readCloudagePtr_->GetInfo()->state;
		qxzsy.cloud_state = state == WMCA_NO_DATA ? 1 : (state == WMCA_TOO_OLD ? 2 : 0);
	}

	const InfoCloudage* nfCloudage = readCloudagePtr_->GetInfo();
	int zone_count = nfCloudage->zones.size();
	int pack_count = (zone_count + zone_max - 1) / zone_max;
	if (qxzsy.cloud_state == 0) {// 填充云量分布基本信息
		UTC2DateTimeBJ(nfCloudage->utc.c_str(), qxzsy.cloud_date, qxzsy.cloud_time);
		qxzsy.azi_step   = int32_t(nfCloudage->azStep * 10);
		qxzsy.alt_step   = int32_t(nfCloudage->elStep * 10);
		qxzsy.pack_count = pack_count;
	}

    // 填充气象信息
	if (weaStatPtr_.unique() && weaStatPtr_->IsRun()) {
		const InfoWeather* nfWea = weaStatPtr_->GetInfo();
        if (nfWea->state == WEA_NO_DATA) {
            qxzsy.wea_state = 2;
        }
		else {
			qxzsy.wea_state = 0;
			UTC2DateTimeBJ(nfWea->utc.c_str(), qxzsy.wea_date, qxzsy.wea_time);
			qxzsy.temp     = int16_t(nfWea->temperature * 10);
			qxzsy.humidity = int16_t(nfWea->humidity * 10);
			qxzsy.airpres  = int16_t(nfWea->pressure * 10);
			qxzsy.windspd  = int16_t(nfWea->windSpeed * 10);
			qxzsy.winddir  = int16_t(nfWea->windOrient * 10);
			qxzsy.rainfall = int16_t(nfWea->rainFall);
		}
	}
	else {
        qxzsy.wea_state = 0x01;
	}

	// 填充夜天光
	if (!sqmPtr_.unique()) {// 未工作
		qxzsy.sqm_state = 0x02;
    }
	else if (!sqmPtr_->IsConnected()) {// 连接失败
		qxzsy.sqm_state = 0x01;
    }
	else {
		const InfoSQM* nfSQM = sqmPtr_->GetInfo();
		if (nfSQM->state == SQM_NO_DATA) {
            qxzsy.sqm_state = 0x03; // 无读出
        }
		else {// 正常
			qxzsy.sqm_state = 0;
			UTC2DateTimeBJ(nfSQM->utc.c_str(), qxzsy.sqm_date, qxzsy.sqm_time);
			qxzsy.sqm_bkmag = int16_t(nfSQM->mpsas * 100);
		}
	}

	// 分包填充云量分布, 并尝试发送
	int byteData0(byteQXZSY - bytePerCloudage); // 气象自适应信息 - 单天区云量
	if (qxzsy.cloud_state) {// 无云量分布
		qxzsy.cloud_percent = UINT16_MAX;
		qxzsy.len = byteData0 - sizeof(FrameHead);
		udp->WriteTo(&qxzsy, byteData0, ip, port);
	}
	else {// 有云量分布
		int zoneWritten(0); // 已发送天区云量
		int zoneWrite;   // 待发送天区云量
		const CloudAgeSet& caSet = nfCloudage->zones;
		boost::shared_array<PDXP_Cloudage> caArray(new PDXP_Cloudage[zone_max]);

		// 统计全天云量
		int zone_count = caSet.size();
		int zone_greater_7(0);
		for (int i = 0; i < zone_count; ++i) {
			if (std::get<2>(caSet[i]) >= 7) ++zone_greater_7;
		}
		qxzsy.cloud_percent = uint16_t(zone_greater_7 * 1000 / zone_count);

		// 填充分天区云量
		for (int pack_no = 1; pack_no <= pack_count; ++pack_no) {
			// 填充云量数据
			if ((zoneWrite = zone_count - zoneWritten) > zone_max) zoneWrite = zone_max;

			for (int j = 0; j < zoneWrite; ++j, ++zoneWritten) {
                caArray[j].azi   = int32_t(std::get<0>(caSet[zoneWritten]) * 10);
                caArray[j].alt   = int32_t(std::get<1>(caSet[zoneWritten]) * 10);
                caArray[j].level = int16_t(std::get<2>(caSet[zoneWritten]));
			}
			// 补全协议
			qxzsy.len = byteData0 - sizeof(FrameHead) + zoneWrite * bytePerCloudage;
			qxzsy.zone_count = zoneWrite;
			qxzsy.pack_no = pack_no;
			char *ptr = bufWrite.get();
			memcpy(ptr, &qxzsy, byteData0);  ptr += byteData0;
			memcpy(ptr, caArray.get(), zoneWrite * bytePerCloudage);
			udp->WriteTo(bufWrite.get(), qxzsy.len + sizeof(FrameHead), ip, (uint16_t) port);
		}
	}
}

// 功能
/**
 * @brief 处理收到的UDP信息: <-- command
 */
void EnvMonitor::udp_receive_command(const char* rcvd, const int bytes) {
	if (!camCloudPtr_.unique()) {
		_gLog.Write(LOG_WARN, "Cloud camera is not working, rejected focus command");
		return ;
	}

	// 2023-10-15: 仅接受调焦信息
	ProtoFocusBase* basis = (ProtoFocusBase*) rcvd;
	if (basis->check != FOCUS_CHECK_CODE) return ;

	switch (basis->type)
	{
	case TYPE_FOCUS_BEGIN:
		camCloudPtr_->DoFocus(udpCmd_, true, ((const ProtoFocusBegin*) rcvd)->manual);
		break;
	case TYPE_FOCUS_END:
		camCloudPtr_->DoFocus(udpCmd_);
		break;
	case TYPE_FOCUS_MOVE:
		{
			ProtoFocusMove* proto = (ProtoFocusMove*) rcvd;
			camCloudPtr_->FocusMove(proto->step);
			_gLog.Write("Focou[Move]: %d", proto->step);
		}
		break;
	case TYPE_FOCUS_LIMIT:
		camCloudPtr_->FocusTargetOverLimit();
		break;
	default:
		_gLog.Write(LOG_WARN, "undefined focus protocol [type = 0x%0X]", basis->type);
		break;
	}
}

void EnvMonitor::focus_respond(const int rslt, const int value) {
	if (udpCmd_.unique() && (rslt == 0 || rslt == 1)) {
		if (rslt == 0) {
			ProtoFocusMove proto;
			proto.step = value;
			udpCmd_->Write(&proto, sizeof(ProtoFocusMove));
		}
		else {
			ProtoFocusEnd proto;
			proto.success = value > 0;
			proto.fwhm    = value;
			udpCmd_->Write(&proto, sizeof(ProtoFocusMove));
		}
	}
}

const char* EnvMonitor::log_filepath(const InfoCloudage* info_) {
    try {
        // 文件路径
        // <root>
        //       CloudAge
        //               Y<year>
        //                      CA<year><month><day>
        //                                          CA<year><month><day>T<hour><minute><second>.json
        //
        ptime utc = from_iso_extended_string(info_->utc);
        ptime::date_type day = utc.date();
        ptime::time_duration_type tdt = utc.time_of_day();
        static path pathName;
        pathName = param_->sampleDir;
        pathName /= "WeaFile";
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
        }
        pathName /= (boost::format("Y%d") % day.year()).str();
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
        }
        pathName /= (boost::format("WEA%d%02d%02d") % day.year() % day.month().as_number() % day.day()).str();
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
        }
        pathName /= (boost::format("%d%02d%02d%02d%02d%02d_5606.wea")
                     % day.year() % day.month().as_number() % day.day()
                     % tdt.hours() % tdt.minutes() % tdt.seconds()).str();

        return pathName.c_str();
    }
    catch(filesystem_error& ex) {
        _gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
    }
    catch(...) {
        _gLog.Write(LOG_FAULT, "[%s:%s], wrong time style[%s]", __FILE__, __FUNCTION__, info_->utc.c_str());
    }
    return "";
}
