
#include <boost/date_time/gregorian/greg_date.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include "ProtocolPDXP.h"

using namespace boost::gregorian;
using namespace boost::posix_time;

void String2DateTime(const char* ext_iso_str, int32_t& days, int32_t& fd) {
    ptime tmin = from_iso_extended_string(ext_iso_str);
    days = tmin.date().julian_day() - 2451544;
    fd   = tmin.time_of_day().total_microseconds() / 100;
}

std::string DateTime2String(int32_t days, int32_t fd) {
    boost::format fmt("%sT%02d:%02d:%02d.%04d");
    date_duration dd(days - 1);
    date ymd = date(2000, Jan, 1) + dd;
    int ms = fd % 10000;
    fd = (fd - ms) / 10000;
    int ss = fd % 60;
    fd = (fd - ss) / 60;
    int mm = fd % 60;
    int hh = (fd - mm) / 60;
    fmt % to_iso_extended_string(ymd) % hh % mm % ss % ms;
    return fmt.str();
}

std::string Time2String(int32_t fd) {
    boost::format fmt("%02d:%02d:%02d.%04d");
    int ms = fd % 10000;
    fd = (fd - ms) / 10000;
    int ss = fd % 60;
    fd = (fd - ss) / 60;
    int mm = fd % 60;
    int hh = (fd - mm) / 60;
    fmt % hh % mm % ss % ms;
    return fmt.str();
}

void UTC2DateTimeBJ(const char* ext_iso_str, int32_t& days, int32_t& fd) {
    ptime tmin = from_iso_extended_string(ext_iso_str) + hours(8);
    days = tmin.date().julian_day() - 2451544;
    fd   = tmin.time_of_day().total_microseconds() / 100;
}

void DateTime2YMD(int32_t days, int32_t fd, int& year, int&month, double& day)
{
    // 天的小时部分
    double dfd = fd * 1E-4 / 86400.;
    // 日历
    date_duration dd(days - 1);
    date ymd = date(2000, Jan, 1) + dd;
    year  = ymd.year();
    month = ymd.month().as_number();
    day   = ymd.day() + dfd;
}

void DateTime2YD(int32_t days, int32_t fd, int& year, double& ydays) {
    // 天的小时部分
    double dfd = fd * 1E-4 / 86400.;
    // 日历
    date_duration dd(days - 1);
    date ymd = date(2000, Jan, 1) + dd;
    year  = ymd.year();
	ydays = ymd.day_of_year() + dfd;
}

void YD2DateTime(int year, double ydays, int32_t &days, int32_t &fd) {
	int yday = int(ydays);

	fd = int32_t((ydays - yday) * 8.64 * 1E8 + 0.5); // 0.1毫秒
	date ymd2k(2000, Jan, 1);
	date ymd(year, Jan, 1);
	date_duration dd(yday - 1);
	ymd += dd;
	days = (ymd - ymd2k).days() + 1;
}

void Now2DateTimeBJ(int32_t& days, int32_t& fd) {
    ptime now = microsec_clock::local_time();
    days = now.date().julian_day() - 2451544;
    fd   = now.time_of_day().total_microseconds() / 100;
}

int32_t Degree2Int(double deg) {
    uint32_t max = 1 << 31;
    return int32_t (max * deg / 360. + 0.5);
}

double Int2Degree(int32_t val) {
    uint32_t max = 1 << 31;
	return val * 360.0 / max;
}
