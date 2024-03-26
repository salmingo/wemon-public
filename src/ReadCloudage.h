/**
 * @file ReadCloudAge.h 从处理结果中读取云量分布
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief
 * - 监测云量处理结果文件
 * - 从文件中解析
 * @version 0.1
 */

#ifndef CLOUDAGE_H
#define CLOUDAGE_H

#include <tuple>
#include <vector>
#include "BoostInclude.h"
#include "Parameter.h"

typedef std::tuple<float, float, int> CloudAge;
typedef std::vector<CloudAge> CloudAgeSet;

enum {
	WMCA_SUCCESS,	///< 正确
	WMCA_NO_DATA,	///< 无处理结果
	WMCA_TOO_OLD	///< 处理结果太老: 与当前时间超过5分钟
};

struct InfoCloudage {
    int state;         ///< 工作状态. 0: 正确; 1: 无处理结果; 2: 处理结果太老
    string id;         ///< 设备编号
    string utc;        ///< 最后一次处理结果的UTC时间
    double siteLon;    ///< 地理经度, 角度, 东经为正
    double siteLat;    ///< 地理纬度, 角度, 北纬为正
    double siteAlt;    ///< 海拔, 米
    float azStep;      ///< 方位步长
    float elStep;      ///< 高度步长
    CloudAgeSet zones; ///< 全天云量分布

public:
    InfoCloudage() = default;
    void Reset() {
        siteLon = siteLat = siteAlt = __DBL_MAX__;
        azStep = elStep = __FLT_MAX__;
        zones.clear();
    }
};

class ReadCloudage
{
public:
    typedef boost::shared_ptr<ReadCloudage> Pointer;

public:
    ReadCloudage();
    ~ReadCloudage();
    static Pointer Create() {
        return Pointer(new ReadCloudage());
    }

public:
    /**
     * @brief 启动服务
     */
    void Start(const Parameter* param);
    const InfoCloudage* GetInfo() {
        return &info_;
    }

protected:
    /**
     * @brief 线程: 扫描数据处理结果, 生成全天云量分布
     */
    void run();
    /**
     * @brief 从数据处理结果文件中读取/解析云量分布
     * @param filePath 交换文件路径
     * @return 文件读取/解析结果
     */
    bool resolve_file(const char* filePath);
    /**
     * @brief 获取日志文件路径
     * @return 日志文件路径
     * @note
     * - 创建文件目录
     * - 获取文件路径
     */
    const char* log_filepath();
    /**
     * @brief 将单帧图像处理结果以JSON格式写入日志文件
     */
    void save_log();

private:
	const Parameter* param_; ///< 配置参数
    InfoCloudage info_;     ///< 云量分布信息
    ThrdPtr thrdMain_;      ///< 线程指针
};

typedef ReadCloudage::Pointer ReadCloudagePtr;

#endif
