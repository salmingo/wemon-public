/**
 * @file CameraQHY.h  QHY相机控制接口声明文件
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief 继承CameraBase, 实现对QHY CMOS相机控制的封装
 * @version 1.1
 * @date 2023-01-03
 */

#ifndef _SRC_CAMERA_QHY_H_
#define _SRC_CAMERA_QHY_H_

#include "qhyccd.h"
#include "../CameraBase.h"

class CameraQHY : public CameraBase
{
public:
    CameraQHY();
    ~CameraQHY();

protected:
	/* 成员变量 */
	qhyccd_handle *hcam_;	//< 相机SDK访问句柄
	ThrdPtr thrdWaitFrm_;	//< 线程: 等待曝光结束或中止或失败
	boost::condition_variable cvWaitFrm_;	///< 事件: 开始曝光

protected:
	/**
	 * @brief 线程: 等待曝光正常或异常结束
	 */
	void thread_wait_frame();

protected:
	/* 功能 */
	/**
	 * @brief 建立与相机的连接
	 * @return 连接结果
	 */
	bool open_camera();
	/**
	 * @brief 断开与相机的连接
	 */
	void close_camera();
	/**
	 * @brief 启动/停止制冷
	 * @param onoff      true: 启动; false: 停止
	 * @param coolerSet  制冷温度, 摄氏度
	 */
	void cooler_onoff(bool onoff, int coolerSet);
	/**
	 * @brief 获得芯片温度
	 * @return 芯片温度, 摄氏度
	 * @note
	 * 温度采集错误时, 代表相机异常, 需要做故障诊断
	 */
	bool sensor_temperature(int& temperature);
	/**
	 * @brief 设置曝光时间
	 * @param expdur   曝光时间, 秒
	 * @return 操作执行结果
	 */
	bool set_expdur(double expdur);
	/**
	 * @brief 启动曝光流程
	 * @return 操作执行结果
	 */
	bool start_expose();
	/**
	 * @brief 停止曝光流程
	 * @return 操作执行结果
	 */
	bool stop_expose();
	/**
	 * @brief 设置感兴趣窗口
	 */
	bool set_ROI(int x0, int y0, int w, int h, int xbin, int ybin);
	/**
	 * @brief 设置AD通道
	 * @param index     档位索引
	 * @param bitdepth  像元位宽
	 * @return 操作结果成功标志
	 */
	bool set_ADChannel(uint16_t index, uint16_t &bitdepth);
	/**
	 * @brief 设置读出端口
	 * @param index     档位索引
	 * @param value     读出端口名称
	 * @return 操作结果成功标志
	 */
	bool set_ReadPort(uint16_t index, string& value);
	/**
	 * @brief 设置读出速度
	 * @param index     档位索引
	 * @param value     读出速度名称
	 * @return 操作结果成功标志
	 */
	bool set_ReadRate(uint16_t index, string& value);
	/**
	 * @brief 设置前置增益
	 * @param index     档位索引
	 * @param gain      增益数值
	 * @return 操作结果成功标志
	 */
	bool set_gain_preamp(uint16_t index, float& gain);
	/**
	 * @brief 设置行转移速度
	 * @param index     档位索引
	 * @param rate      速度值
	 * @return 操作结果成功标志
	 */
	bool set_vershift(uint16_t index, float& rate);
	/**
	 * @brief 设置AD通道
	 * @param onoff     启用/停用EM模式
	 * @param gain      EM增益
	 * @return 操作结果成功标志
	 */
	bool set_gain_em(bool onoff, uint16_t gain);
	/**
	 * @brief 从ROM中读取相机的可配置参数
	 */
	bool init_parameters();
	/**
	 * @brief 从文件中加载相机的可配置参数
	 */
	void load_parameters();
};

#endif