
#include <fstream>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include "ReadCloudage.h"
#include "GLog.h"

using namespace boost;
using namespace boost::filesystem;
using namespace boost::posix_time;

typedef std::vector<string> strvec;

ReadCloudage::ReadCloudage() {
}

ReadCloudage::~ReadCloudage() {
    interrupt_thread(thrdMain_);
}

void ReadCloudage::Start(const Parameter* param) {
    param_ = param;
    thrdMain_.reset(new boost::thread(boost::bind(&ReadCloudage::run, this)));
}

void ReadCloudage::run() {
    boost::chrono::seconds period(1);
    path pathFile(param_->sampleDir);
    pathFile /= param_->fileCloudAge;
    std::time_t lastTime(0), oldTime(0);
    int ellapsed(0);
    bool update(false);

    while (1) {
        boost::this_thread::sleep_for(period);

        if (!exists(pathFile)) {// 无文件
            info_.state = WMCA_NO_DATA;
        }
        else if ((lastTime = last_write_time(pathFile)) != oldTime) {// 初次读文件或文件已更新
            oldTime = lastTime;
            update = true;
        }
        else if (update) {// 解析文件
            update = false;
            ellapsed = 0;
            info_.state = WMCA_SUCCESS;
            if (resolve_file(pathFile.c_str())) save_log();
        }
        else if (++ellapsed > 300 && info_.state != WMCA_TOO_OLD) {// 5分钟文件未更新
            info_.state = WMCA_TOO_OLD;
        }
    }
}

bool ReadCloudage::resolve_file(const char* filePath) {
    strvec tokens;
    char lnbuf[100];
    int lno(0);
    std::ifstream ifs(filePath);
    string line;

    info_.Reset();
    while (!ifs.eof() && ifs.getline(lnbuf, 100)) {
        line = lnbuf;
        trim(line);
        if (line[0] == '#') {// 注释
            split(tokens, line, boost::is_any_of(" \t="), boost::token_compress_on);

            if (tokens.size() < 2) continue;
            if (iequals(tokens[1], "ID")) info_.id = tokens[2];
            else if (iequals(tokens[1], "SITE")) {
                info_.siteLon = std::stof(tokens[2]);
                info_.siteLat = std::stof(tokens[3]);
                info_.siteAlt = std::stof(tokens[4]);
            }
            else if (iequals(tokens[1], "STEP")) {
                info_.azStep = std::stof(tokens[2]);
                info_.elStep = std::stof(tokens[3]);
            }
        }
        else {
            if (++lno == 1) info_.state = std::stoi(line); // 第一行
            else if (lno == 2) info_.utc = line; // 第二行
            else {// 第三行至文件结束
                split(tokens, line, boost::is_any_of(" \t"), boost::token_compress_on);
                float azi = std::stof(tokens[0]);
                float ele = std::stof(tokens[1]);
                int level = std::stoi(tokens[2]);
//                if (ele >= 29) info_.zones.push_back(std::make_tuple(azi, ele, level));
                info_.zones.push_back(std::make_tuple(azi, ele, level));
            }
        }
    }
	std::stable_sort(info_.zones.begin(), info_.zones.end(), [](const CloudAge& x1, const CloudAge& x2) {
		return (get<1>(x1) > get<1>(x2) || (get<1>(x1) == get<1>(x2) && get<0>(x1) <= get<0>(x2)));
	});

    return (info_.state == 0 && info_.azStep < __FLT_MAX__ && info_.elStep < __FLT_MAX__);
}

const char* ReadCloudage::log_filepath() {
    try {
        // 文件路径
        // <root>
        //       CloudAge
        //               Y<year>
        //                      CA<year><month><day>
        //                                          CA<year><month><day>T<hour><minute><second>.json
        //
        ptime utc = from_iso_extended_string(info_.utc);
        ptime::date_type day = utc.date();
        ptime::time_duration_type tdt = utc.time_of_day();
        static path pathName;
        pathName = param_->sampleDir;
        pathName /= "CloudAge";
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
        }
        pathName /= (boost::format("Y%d") % day.year()).str();
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
        }
        pathName /= (boost::format("CA%d%02d%02d") % day.year() % day.month().as_number() % day.day()).str();
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::group_write | perms::others_write | perms::remove_perms);
        }
        pathName /= (boost::format("CA%d%02d%02dT%02d%02d%02d.json")
            % day.year() % day.month().as_number() % day.day()
            % tdt.hours() % tdt.minutes() % tdt.seconds()).str();

        return pathName.c_str();
    }
    catch(filesystem_error& ex) {
        _gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
    }
    catch(...) {
        _gLog.Write(LOG_FAULT, "[%s:%s], wrong time style[%s]", __FILE__, __FUNCTION__, info_.utc.c_str());
    }
    return "";
}

void ReadCloudage::save_log() {
    string pathName = log_filepath();
    if (pathName.empty()) return ;

    try {
        boost::property_tree::ptree pt;
        pt.add("ID",      info_.id);
        pt.add("state",   info_.state);
        pt.add("utc",     info_.utc);
        if (info_.siteLon < __DBL_MAX__ && info_.siteLat < __DBL_MAX__ && info_.siteAlt < __DBL_MAX__) {
            boost::property_tree::ptree& ptSite = pt.add("GeoSite", "");
            ptSite.add("Longitude", info_.siteLon);
            ptSite.add("Latitude",  info_.siteLat);
            ptSite.add("Altitude",  info_.siteAlt);
        }
        pt.add("Step.Azimuth",    info_.azStep);
        pt.add("Step.Elevation",  info_.elStep);
        CloudAgeSet& zones = info_.zones;
        int n = (int) zones.size();
        for (int i = 0; i < n; ++i) {
            boost::property_tree::ptree& ptZone = pt.add("distribution", "");
            ptZone.add("azi",   std::get<0>(zones[i]));
            ptZone.add("ele",   std::get<1>(zones[i]));
            ptZone.add("level", std::get<2>(zones[i]));
        }
        boost::property_tree::write_json(pathName, pt);
    }
    catch(boost::property_tree::json_parser_error& ex) {
        _gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
    }
}
