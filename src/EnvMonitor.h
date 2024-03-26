
#ifndef _SRC_ENV_MONITOR_H_
#define _SRC_ENV_MONITOR_H_

#include "Parameter.h"
#include "SQM.h"
#include "WeatherStation.h"
#include "ReadCloudage.h"
#include "AsioUDP.h"
#include "CloudCamera.h"

class EnvMonitor {
public:
	EnvMonitor(const Parameter* param);
	~EnvMonitor();

public:
	/* 接口 */
	/**
	 * @brief 启动服务
	 * @return 服务启动结果
	 */
	bool Start();
	/**
	 * @brief 停止服务
	 */
	void Stop();

protected:
	/**
	 * @brief 监测/计算每日的晨昏时, 启动/停止服务
	 */
	void monitor_twilight();
	/**
	 * @brief 线程: 监视磁盘空间并清理历史数据
	 */
	void thread_diskfree();
	/**
	 * @brief 线程: 定时上传PDXP格式数据
	 */
	void thread_pdxp();
	/**
	 * @brief 上传PDXP接口数据
	 * @param udp  UDP接口
	 * @param pno  帧序号
	 */
	void upload_pdxp(UdpPtr udp, uint32_t pno, const char* ip, int port);

    /**
     * @brief 生成事后气象数据文件
     */
    void save_json();

// 功能
protected:
	/**
	 * @brief 处理收到的UDP信息: <-- command
	 * @param rcvd  信息
	 * @param bytes 信息长度
	 */
	void udp_receive_command(const char* rcvd, const int bytes);
	/**
	 * @brief 调焦回调函数
	 * @param rslt   0: 继续调焦; 1: 调焦结束
	 * @param value  rslt==0时, value=调焦步长; ==1时, value=fwhm*100
	 */
	void focus_respond(const int rslt, const int value);

    /**
     * @brief 获取日志文件路径
     * @return 日志文件路径
     */
    const char* log_filepath(const InfoCloudage* info_);

protected:
	const Parameter* param_; ///< 配置参数
	int odt_;	///< 观测时段类型

	// 设备接口
	SQMPtr sqmPtr_;			///< SQM接口
	WeaStatPtr weaStatPtr_;	///< 气象站接口
	ReadCloudagePtr readCloudagePtr_;	///< 读取云量分布接口
	CloudCamPtr camCloudPtr_;	///< 云量相机接口
	// 网络接口
	UdpPtr udpCastPtr_;		///< 组播接口
	UdpPtr udpCmd_;			///< 命令接口

	/* 线程 */
	ThrdPtr thrdTwilight_;	///< 线程: 计算晨昏时作为设备启动/停止时间
	ThrdPtr thrdDisk_;		///< 线程: 监视磁盘空间并清理历史数据
	ThrdPtr thrdPDXP_;		///< 线程: PDXP上传
};

#endif
