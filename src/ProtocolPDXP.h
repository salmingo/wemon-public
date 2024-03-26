/**
 * @file ProtocolPDXP.h 声明通信协议通用包头
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @version 0.1
 * @date 2023-02-04
 */

#ifndef PROTOCOL_PDXP_H_
#define PROTOCOL_PDXP_H_

#include <cstdint>
#include <string.h>
#include <ctype.h>
#include <string>
#include <sstream>
#include <iomanip>

//============================================================================
/**
 * @brief 字符串格式时间转换为日期和时间
 * @param ext_iso_str  扩展ISO格式时间, CCYY-MM-DDThh:mm:ss.sss
 * @param days     日期, 2000年1月1日==1
 * @param fd       时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 */
extern void String2DateTime(const char* ext_iso_str, int32_t& days, int32_t& fd);

/**
 * @brief 日期和时间转换为字符串
 * @param days     日期, 2000年1月1日==1
 * @param fd       时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 * @return 扩展ISO格式时间, CCYY-MM-DDThh:mm:ss.sss
 */
extern std::string DateTime2String(int32_t days, int32_t fd);

/**
 * @brief 时间转换成字符串
 * @param fd      时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 * @return 扩展ISO格式时间, hh:mm:ss.sss
*/
extern std::string Time2String(int32_t fd);

/**
 * @brief 字符串格式时间, 由UTC转换为北京时的日期和时间
 * @param ext_iso_str  扩展ISO格式时间, CCYY-MM-DDThh:mm:ss.sss
 * @param days     日期, 2000年1月1日==1
 * @param fd       时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 */
extern void UTC2DateTimeBJ(const char* ext_iso_str, int32_t& days, int32_t& fd);

/**
 * @brief由日期和时间转换为年月日
 * @param days       日期, 2000年1月1日==1
 * @param fd         时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 * @param year       年
 * @param month      月
 * @param day        天
 */
extern void DateTime2YMD(int32_t days, int32_t fd,
    int& year, int&month, double& day);
/**
 * @brief 由日期和时间转换为年和年内天数
 * @param days       日期, 2000年1月1日==1
 * @param fd         时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 * @param year       年
 * @param ydays      年内天数
 */
extern void DateTime2YD(int32_t days, int32_t fd, int& year, double& ydays);
/**
 * @brief 由年(-2000)和年天数转换为日期和时间
 * @param year       年
 * @param ydays      年内天数
 * @param days       日期, 2000年1月1日==1
 * @param fd         时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 */
extern void YD2DateTime(int year, double ydays, int32_t &days, int32_t &fd);

/**
 * @brief 当前本地时对应的日期和时间
 * @param bjdate       日期, 2000年1月1日==1
 * @param bjtime       时间, 当日本地时0时为零点, 量纲: 0.1毫秒
 */
extern void Now2DateTimeBJ(int32_t& bjdate, int32_t& bjtime);

/**
 * @brief 角度转换为整数
 * @param deg   角度, [-180, +360)
 * @return 对应的整数
 * @note
 * return = deg * 2^31 / 360
 */
extern int32_t Degree2Int(double deg);
/**
 * @brief 角度转换为双精度实数
 * @param val   整数
 * @return 角度, [-180, +360)
 * @note
 * return = val * 360 / 2^31
 */
extern double Int2Degree(int32_t val);

//============================================================================
#define SID     	0x50001001	// 5000-气象信息; 100-站址; 1-设备编号
#define BID_QXZSY   0x50000001  // 5000-气象信息; 0001-自适应气象

//============================================================================
/* 以结构体定义通信协议 */
#pragma pack(push, 1)

/**
 * @brief 协议帧头
 * @note
 * - 新建帧头时, 需要设置bid, pno, len
 * - 更新帧头时, 需要设置date, time和pno
 */
struct FrameHead {
    uint16_t ver;   ///< 协议版本, 默认: 8080H
    uint32_t sid;   ///< 信源
    uint32_t bid;   ///< 数据标志, 构成: 类别+类+子类
    uint32_t pno;   ///< 相同bid的包序号. 初始值: 0
    uint16_t len;	///< 数据区长度

public:
    FrameHead() {
        ver  = 0x8080;
        sid  = SID;
        bid  = 0;
		pno  = 1;
    }

    std::string ToString() const {
        std::stringstream ss;
		ss << "FrameHead: ";
        ss << ", PNO = "      << pno;
        ss << ", LEN = "      << len;
        ss << std::hex;
        ss << ", BID = 0x"    << bid;
        ss << ", SID = 0x"    << sid;
		ss << std::endl;
        return ss.str();
    }
};

// #define BID_QXZSY       0x50004102  // 气象自适应
struct PDXP_Cloudage {// 云量
    int32_t azi;    ///< 中心方位, 0.1度
    int32_t alt;    ///< 中心俯仰, 0.1度
    int16_t level;  ///< 分级. 0-9. 0==最好

public:
    PDXP_Cloudage() = default;

    std::string ToString() const {
        std::stringstream ss;
        ss << azi << ", " << alt << ", " << level;
        return ss.str();
    }
};

struct PDXP_QXZSY : public FrameHead {// 气象自适应信息
    int32_t date;       ///< 数据日期, 相对2000-01-01的累计日期, 2000-01-01对应值==1
    int32_t time;       ///< 数据时间, 相对当日0时. 量纲: 0.1毫秒
    uint8_t wea_state;  ///< 气象站状态. 00: 正确; 01: 未连接; 02: 无反馈
    int32_t wea_date;   ///< 采集日期, 相对2000-01-01的累计日期, 2000-01-01对应值==1
    int32_t wea_time;   ///< 采集时间, 相对当日0时. 量纲: 0.1毫秒
    int16_t temp;       ///< 温度, 0.1℃, 补码
    uint16_t humidity;  ///< 相对湿度, 0.1%
    uint16_t airpres;   ///< 大气压, 0.1百帕
    uint16_t windspd;   ///< 风速, 0.1m/s
    uint16_t winddir;   ///< 风向, 0.1°, 正北==0
    uint16_t rainfall;  ///< 降雨. 0: 无雨. >0: 有雨; <0: 错误
	uint16_t cloud_percent;	///< 全天云量分布
    uint8_t sqm_state;  ///< SQM状态. 00: 正确; 01: 未连接; 02: 无反馈
    int32_t sqm_date;   ///< 采集日期, 相对2000-01-01的累计日期, 2000-01-01对应值==1
    int32_t sqm_time;   ///< 采集时间, 相对当日0时. 量纲: 0.1毫秒
    int16_t sqm_bkmag;  ///< 背景亮度, 0.01MV, 补码
    uint8_t cloud_state;    ///< 云量状态. 00: 正确; 01: 未连接; 02: 无反馈
    int32_t cloud_date;     ///< 采集日期, 相对2000-01-01的累计日期, 2000-01-01对应值==1
    int32_t cloud_time;     ///< 采集时间, 相对当日0时. 量纲: 0.1毫秒
    uint16_t zone_count;    ///< 天区数量
    uint32_t azi_step;      ///< 方位步长, 0.1度
    uint32_t alt_step;      ///< 俯仰步长, 0.1度
    uint16_t pack_count;    ///< 数据子帧个数
    uint16_t pack_no;       ///< 数据子帧序号
    PDXP_Cloudage cloud[1]; ///< 各天区云量

public:
    PDXP_QXZSY() {
		bid = BID_QXZSY;

        Now2DateTimeBJ(date, time);
        wea_state = UINT8_MAX;
        wea_date = wea_time = INT32_MAX;
        temp = INT16_MAX;
        humidity = airpres = windspd = winddir = rainfall = INT16_MAX;
        sqm_state = 0xFF;
        sqm_date = sqm_time = INT32_MAX;
        sqm_bkmag = INT16_MAX;
        cloud_state = UINT8_MAX;
        cloud_date = cloud_time = INT32_MAX;
        zone_count = 0;
        azi_step = INT32_MAX;
        alt_step = INT32_MAX;
        pack_count = 0;
        pack_no = 1;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << "DATETIME = " << DateTime2String(date, time);
        ss << std::hex;
        ss << ", STAT WEA = "   << std::setfill('0') << std::setw(2) << (uint32_t) wea_state;
        ss << ", STAT SQM = "   << std::setfill('0') << std::setw(2) << (uint32_t) sqm_state;
        ss << ", STAT CLOUD = " << std::setfill('0') << std::setw(2) << (uint32_t) cloud_state;
        ss << std::dec;
        ss << ", TIME WEA = "   << DateTime2String(wea_date, wea_time);
        ss << ", TEMP = "       << temp;
        ss << ", HUM = "        << humidity;
        ss << ", AIRP = "       << airpres;
        ss << ", WIND SPD = "   << windspd;
        ss << ", WIND DIR = "   << winddir;
        ss << ", RAIN = "       << rainfall;
        ss << ", TIME SQM = "   << DateTime2String(sqm_date, sqm_time);
        ss << ", BK = "         << sqm_bkmag;
        ss << ", TIME CLOUD = " << DateTime2String(cloud_date, cloud_time);
        ss << ", ZONE COUNT = " << zone_count;
        ss << ", STEP AZI = "   << azi_step;
        ss << ", STEP ALT = "   << alt_step;
        ss << ", PCK COUNT = "  << pack_count;
        ss << ", PCK NO = "     << pack_no;
        for (int i = 0; i < zone_count; i++) {
            ss << ", ZONE[" << std::setw(3) << i << "] = ";
            ss << cloud[i].ToString();
        };
        return ss.str();
    };
};

#pragma pack(pop)

#endif
