
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "Parameter.h"
#include "GLog.h"

using namespace boost::property_tree;
using namespace boost::algorithm;
using namespace boost::posix_time;

Parameter::Parameter() {
	devID     = "01";
	siteName  = "Hainan";
	siteLon   = 109.62514;
	siteLat   = 18.34;
	siteAlt   = 44;

	addrMulticast = "224.1.1.10";	///< 组播地址
	portMulticast = 5000;	///< 组播端口
	codeMulticast = 1;		///< 组播信息编码格式: 1: JSON; 2: Struct - 西光
	portCommand   = 5001;	///< UDP服务: 响应控制指令

	enablePDXP = false;
	addrPDXP = "233.1.1.11";
	portPDXP = 6000;
	addrPDXP1 = "233.1.1.12";
	portPDXP1 = 6010;

	/* 智能PDU */
	addrPDU = "192.168.1.2";		///< PDU地址
	portPDU = 3002;		///< PDU端口
	portDevice = 5;		///< 设备电源在PDU上的端口

	/* 采样周期 */
	sampleCycle = 30;	///< 采样周期
	sampleDir = "/history";	///< 测量数据存储目录

	/* 气象站 */
	portWeaStation = "/dev/ttyUSB0";	///< 气象站串口名称
	rainEnable = true;
	portRain = "/dev/tty.usbserial-B001LGNN";

	/* SQM */
	sqmEnable = true;
	addrSQM = "192.168.1.6";		///< SQM地址

	/* 云量相机 */
	fileCloudAge= "updateFile_new.txt";
	dirRawImage = "/data";	///< 目录名称
	prefixName  = "WMC";	///< 目录与文件名前缀
	sunEleMax   = -10;	///< 太阳仰角上限, 角度
	expdurMin   = 1;		///< 最短曝光时间, 秒. >= 0
	expdurMax   = 10;	///< 最大曝光时间, 秒
	saturation  = 60000;	///< 饱和值
	coolerSet   = -10;	///< 制冷温度
	minDiskFree = 100;	///< 可用空间小于100GB时删除历史数据
	fwhmPerfect = 3.0;	///< 期望FWHM值
}

Parameter::~Parameter() {
}

bool Parameter::Init(const char* filePath) {
	return Save(filePath);
}

bool Parameter::Load(const char* filePath) {
	try {
		ptree pt;
		read_xml(filePath, pt);

		for (ptree::iterator it = pt.begin(); it != pt.end(); ++it) {
			if (iequals(it->first, "Device")) {
				devID = it->second.get("<xmlattr>.ID", "01");
			}
			else if (iequals(it->first, "GeoSite")) {
				siteName = it->second.get("<xmlattr>.Name", "Hainan");
				siteLon  = it->second.get("Location.<xmlattr>.Longitude", 0.0);
				siteLat  = it->second.get("Location.<xmlattr>.Latitude", 0.0);
				siteAlt  = it->second.get("Location.<xmlattr>.Altitude", 0.0);
			}
			else if (iequals(it->first, "Network")) {
				addrMulticast = it->second.get("Multicast.<xmlattr>.Address", "224.1.1.10");
				portMulticast = it->second.get("Multicast.<xmlattr>.Port",    3000);
				codeMulticast = it->second.get("Code.<xmlattr>.Type",    1);
				portCommand   = it->second.get("Command.<xmlattr>.Port", 3001);
				enablePDXP    = it->second.get("PDXP.<xmlattr>.Enable", false);
				addrPDXP      = it->second.get("PDXP.<xmlattr>.Address", "233.1.1.11");
				portPDXP      = it->second.get("PDXP.<xmlattr>.Port",    6000);
				addrPDXP1     = it->second.get("PDXP1.<xmlattr>.Address", "233.1.1.12");
				portPDXP1     = it->second.get("PDXP1.<xmlattr>.Port",    6010);
			}
			else if (iequals(it->first, "PDU")) {
				addrSQM    = it->second.get("IP.<xmlattr>.Address", "192.168.100.2");
				portPDU    = it->second.get("IP.<xmlattr>.Port",    3002);
				portDevice = it->second.get("DevicePower.<xmlattr>.Port", 5);
			}
			else if (iequals(it->first, "Sample")) {
				sampleCycle = it->second.get("<xmlattr>.Cycle", 30);
				sampleDir   = it->second.get("<xmlattr>.Dir", "/history");
				if (sampleCycle < 20) sampleCycle = 20;
				else if (sampleCycle > 60) sampleCycle = 60;
			}
			else if (iequals(it->first, "WeatherStation")) {
				portWeaStation = it->second.get("<xmlattr>.Port", "/dev/ttyUSB0");
				rainEnable = it->second.get("Rain.<xmlattr>.Enable", false);
				portRain   = it->second.get("Rain.<xmlattr>.Port", "/dev/tty.usbserial-B001LGNN");
			}
			else if (iequals(it->first, "SQM")) {
				sqmEnable = it->second.get("<xmlattr>.Enable", false);
				addrSQM = it->second.get("<xmlattr>.Address", "192.168.100.6");
			}
			else if (iequals(it->first, "CloudCamera")) {
				fileCloudAge = it->second.get("CloudAge.<xmlattr>.FileName", "");
				dirRawImage  = it->second.get("Storage.<xmlattr>.Dir",      "/data");
				prefixName   = it->second.get("Storage.<xmlattr>.Prefix",   "WMC");
				sunEleMax    = it->second.get("SunElevation.<xmlattr>.Max", -10);
				expdurMin    = it->second.get("Exposure.<xmlattr>.Min", 1);
				expdurMax    = it->second.get("Exposure.<xmlattr>.Max", 10);
				saturation   = it->second.get("Camera.<xmlattr>.Saturation", 60000);
				coolerSet    = it->second.get("Camera.<xmlattr>.Cooler",     -10);
				minDiskFree  = it->second.get("FreeDisk.<xmlattr>.Min",      100);
				fwhmPerfect  = it->second.get("Focus.<xmlattr>.FWHM",        3.0);
			}
		}

		return true;
	}
	catch(xml_parser_error& ex) {
		_gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
		return false;
	}

	return true;
}

bool Parameter::Save(const char* filePath) {
	try {
		ptree pt;

		pt.add("LastUpdate", to_iso_string(second_clock::local_time()));
		devID = pt.get("Device.<xmlattr>.ID", "01");

		ptree& ptSite = pt.add("GeoSite", "");
		ptSite.add("<xmlattr>.Name", siteName);
		ptSite.add("Location.<xmlattr>.Longitude", siteLon);
		ptSite.add("Location.<xmlattr>.Latitude", siteLat);
		ptSite.add("Location.<xmlattr>.Altitude", siteAlt);

		ptree& ptNetwork = pt.add("Network", "");
		ptNetwork.add("Multicast.<xmlattr>.Address", addrMulticast);
		ptNetwork.add("Multicast.<xmlattr>.Port",    portMulticast);
		ptNetwork.add("Code.<xmlattr>.Type",         codeMulticast);
		ptNetwork.add("Code.<xmlcomment>", "Type 1 : JSON");
		ptNetwork.add("Code.<xmlcomment>", "Type 2 : Struct - Xiguang");
		ptNetwork.add("Command.<xmlattr>.Port",      portCommand);
		ptNetwork.add("PDXP.<xmlattr>.Enable",  enablePDXP);
		ptNetwork.add("PDXP.<xmlattr>.Address", addrPDXP);
		ptNetwork.add("PDXP.<xmlattr>.Port",    portPDXP);
		ptNetwork.add("PDXP1.<xmlattr>.Address", addrPDXP1);
		ptNetwork.add("PDXP1.<xmlattr>.Port",    portPDXP1);

		ptree& ptPDU = pt.add("PDU", "");
		ptPDU.add("IP.<xmlattr>.Address",       addrSQM);
		ptPDU.add("IP.<xmlattr>.Port",          portPDU);
		ptPDU.add("DevicePower.<xmlattr>.Port", portDevice);

		ptree& ptMea = pt.add("Sample", "");
		ptMea.add("<xmlattr>.Cycle", sampleCycle);
		ptMea.add("<xmlattr>.Dir",   sampleDir);

		ptree& ptWeaSta = pt.add("WeatherStation", "");
		ptWeaSta.add("<xmlattr>.Port", portWeaStation);
		ptWeaSta.add("Rain.<xmlattr>.Enable", rainEnable);
		ptWeaSta.add("Rain.<xmlattr>.Port", portRain);

		pt.add("SQM.<xmlattr>.Enable", sqmEnable);
		pt.add("SQM.<xmlattr>.Address", addrSQM);

		ptree& ptCloud = pt.add("CloudCamera", "");
		ptCloud.add("CloudAge.<xmlattr>.FileName", fileCloudAge);
		ptCloud.add("Storage.<xmlattr>.Dir",       dirRawImage);
		ptCloud.add("Storage.<xmlattr>.Prefix",    prefixName);
		ptCloud.add("SunElevation.<xmlattr>.Max",  sunEleMax);
		ptCloud.add("Exposure.<xmlattr>.Min",      expdurMin);
		ptCloud.add("Exposure.<xmlattr>.Max",      expdurMax);
		ptCloud.add("Camera.<xmlattr>.Saturation", saturation);
		ptCloud.add("Camera.<xmlattr>.Cooler",     coolerSet);
		ptCloud.add("FreeDisk.<xmlattr>.Min",      minDiskFree);
		ptCloud.add("Focus.<xmlattr>.FWHM",        fwhmPerfect);

		xml_writer_settings<std::string> settings(' ', 4);
		write_xml(filePath, pt, std::locale(), settings);
		return true;
	}
	catch(xml_parser_error& ex) {
		_gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
		return false;
	}
}
