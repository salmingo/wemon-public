/**
 * @file SQM.h SQM访问接口定义文件
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief 封装SQM(天光背景亮度)访问接口
 * @version 0.1
 * @date 2022-12-23
 * @note
 * 型号: SQM-LE
 * @note
 * 通信接口:
 * 1. TCP: 用于查询实时测量结果, 1-1. 即设备只允许同时存在一条TCP连接
 * 2. UDP: 用于查找网段内的SQM设备
 *  1) 向255.255.255.255或网段广播地址, 端口30718, 发送16进制字符串 00 00 00 F6
 *  2) SQM反馈字符串构成:
 *   起始标志16进制字符串(1-4): 00 00 00 F7
 *   MAC地址(25-30)
 * @note
 * 指令集:
 * 1. rx: 查询测量结果. 反馈数据格式:
 *      亮度     传感器频率     传感器周期1   传感器周期2    温度
 *   r, 06.70m,0000022921Hz,0000000020c,0000000.000s, 039.4C
 *   r,±xx.xxm,xxxxxxxxxxHz,xxxxxxxxxxx,xxxxxxx.xxxs,±xxx.xC
 */

#ifndef _SRC_SQM_H_
#define _SRC_SQM_H_

#include <string>
#include <vector>
#include "BoostInclude.h"
#include "AsioTCP.h"

using std::string;

enum {
    SQM_SUCCESS,
    SQM_FAIL_CONNECT,
    SQM_CLOSED,
    SQM_NO_DATA
};

/**
 * @brief 从气象站读取的环境气象信息
 */
struct InfoSQM {
    int state;      ///< 工作状态. 0: 正确; 1: SQM断网; 2: 无数据反馈
    string utc;     ///< 数据采集时间, CCYY-MM-DDThh:mm:ss
    float mpsas;    ///< 天光背景亮度, 星等@平方角秒

public:
    InfoSQM() = default;
};

/**
 * @brief SQM单元查找结果
 */
struct SQMUnit {
    string ip;  ///< IP地址
    string mac; ///< MAC地址
};
typedef std::vector<SQMUnit> SQMUnitVec;

class SQM {
public:
    typedef boost::shared_ptr<SQM> Pointer;

public:
    SQM(const char* ip, const char* dirName = NULL);
    ~SQM();
    static Pointer Create(const char* ip, const char* dirName = NULL) {
        return Pointer(new SQM(ip, dirName));
    }

protected:
    string  dirRoot_;   ///< 样本数据文件根目录
    string  ipDev_;    ///< 设备IP地址
    const uint16_t portDev_;    ///< 设备端口
    InfoSQM info_;  ///< SQM信息
    FILE* fpLog_;   ///< 日志文件
    int cntRsp_;    ///< 有效采样计数
    int oldDay_;    ///< UTC日期

    TcpCPtr tcpClient_; ///< TCP连接
    ThrdPtr thrdCycle_; ///< 线程指针

/////////////////////////////////////////////////////////////////////
// 静态接口: 查找同一网段的可用NTP
public:
    static SQMUnitVec units_;  ///< 在网段内查找到的设备地址
    static boost::condition_variable cvFound_; ///< UDP查询指令反馈结果

public:
    /**
     * @brief 查找网段内可能存在的设备
     * @return 找到的设备数量
     */
    static int Find();
    /**
     * @brief 查看设备的网络地址
     * @param index  设备索引, 1-count
     * @param ip     IP地址
     * @param mac    MAC地址
     * @return 操作结果的有效性
     */
    static bool GetAddress(int index, string& ip, string& mac);

protected:
	/**
	 * @brief 回调函数: 处理UDP端口上广播收到的信息
	 * @param ec     故障字. 0: 无故障
	 * @param bytes  接收信息的长度, 字节
	 */
	static void handle_found(const boost::system::error_code& ec, const int bytes);
/////////////////////////////////////////////////////////////////////

public:
    /**
     * @brief 启动定时数据查询流程
     * @param cycle 采用周期, 秒
     * @return 启动结果
     */
    bool Start(int cycle);
    /**
     * @brief 检查与SQM的连接是否有效
     * @return 连接有效性
     */
    bool IsConnected();
    /**
     * @brief 查看采样结果
     * @return 数据指针
     */
    const InfoSQM* GetInfo() {
        return &info_;
    }

protected:
    /**
     * @brief 线程: 定时读取天光背景
     * @param cycle 采用周期, 秒
     */
    void run(int cycyle);
    /**
     * @brief 回调函数: 处理收到的SQM信息
     * @param client  网络连接
     * @param ec      故障码
     */
    void handle_receive(TcpClient* client, const boost::system::error_code ec);
    /**
     * @brief  打开日志文件
     * @param  year  UTC年
     * @param  month UTC月
     * @param  day   UTC日
     * @return 文件创建或打开结果
     */
    bool open_file(int year, int month, int day);
};
typedef SQM::Pointer SQMPtr;

#endif
