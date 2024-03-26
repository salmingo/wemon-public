/**
 * @file WeatherStation.h 气象站访问接口定义文件
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief 封装气象站访问接口
 * @version 0.1
 * @date 2022-12-23
 */

#ifndef _SRC_WEATHER_STATION_H_
#define _SRC_WEATHER_STATION_H_

#include <string>
#include "BoostInclude.h"
#include "SerialComm.h"

using std::string;

enum {
    WEA_SUCCESS,
    WEA_FAIL_CONNECT,
    WEA_NO_DATA
};

/**
 * @brief 从气象站读取的环境气象信息
 */
struct InfoWeather {
    int state;      ///< 气象站状态. 0: 正确; 其它: 故障
    string utc;     ///< 数据采集时间, CCYY-MM-DDThh:mm:ss
    float temperature;  ///< 温度, 摄氏度
    float humidity;     ///< 相对湿度, 百分比
    float pressure;     ///< 大气压, 百帕
    float windSpeed;    ///< 风速, 米/秒
    int windOrient;     ///< 风向, 正北=0, 正东=90
    uint32_t rainFall;       ///< 降雨 0 为无降水，1为有降水
};

class WeatherStation {
public:
    typedef boost::shared_ptr<WeatherStation> Pointer;

public:
    WeatherStation(const char*portwea, const char* portrain, const char* dirName = NULL);
    virtual ~WeatherStation();
    static Pointer Create(const char* portwea, const char* portrain, const char* dirName = NULL) {
        return Pointer(new WeatherStation(portwea, portrain, dirName));
    }

protected:
    string  dirRoot_;   ///< 样本数据文件根目录
    string portWea_;   ///< 串口名称: 气象站
	string portRain_;  ///< 串口名称: 雨水
    InfoWeather info_;  ///< 气象信息
    FILE* fpLog_;   ///< 日志文件
    int oldDay_;    ///< UTC日期
    uint32_t oldRainy_;  ///< 雨量
    unsigned char qryType_;   ///< 查询类型

    SerialPtr weaPtr_;  ///< 串口指针: 气象站
	SerialPtr rainPtr_;	///< 串口指针: 雨水
    ThrdPtr thrdQuery_; ///< 线程: 定时发送查询指令
    boost::condition_variable cvGet_;   ///< 事件: 收到反馈

public:
    const InfoWeather* GetInfo() {
        return &info_;
    }
    /**
     * @brief 检查气象站是否工作正常
     * @return 气象站工作状态
     */
    bool IsRun();

public:
    /**
     * @brief 启动数据采集流程
     * @param cycle  采样周期
     * @return 操作执行结果
     */
    bool Start(int cycle);

// 气象站
protected:
    /**
     * @brief 线程: 定时发送查询指令
     * @param cycle  采样周期
     */
    void run(int cycle);
    /**
     * @brief 串口接收信息回调函数
     * @param ec   故障码
     */
    void handle_receive_weather(SerialComm* comm, int ec, size_t bytes);
    /**
     * @brief 延时等待串口反馈
     * @param cntErr   故障计数
     */
    void wait_response(int &cntErr);
    /**
     * @brief  打开日志文件
     * @param  year  UTC年
     * @param  month UTC月
     * @param  day   UTC日
     * @return 文件创建或打开结果
     */
    bool open_file(int year, int month, int day);
    /**
     * @brief 计算机MODBUS协议CRC校验码
     * @param data   待校验数据
     * @param len    数据长度, 字节
     * @return 校验码
     */
    unsigned short modbus_crc16(unsigned char* data, unsigned int len);
    /**
     * @brief 将检验码转换为字符串
     * @param crc   CRC16校验码
     * @param code  字符串
     * @note
     * 校验码高低位转置
     */
    void value2code(unsigned short crc, char* code);

// 雨水
private:
    /**
     * @brief 串口接收信息回调函数
     * @param ec   故障码
     */
    void handle_receive_rain(SerialComm* comm, int ec, size_t bytes);
};

typedef WeatherStation::Pointer WeaStatPtr;

#endif
