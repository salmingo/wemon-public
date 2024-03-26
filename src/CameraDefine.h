/**
 * @file CameraErrorCode.h 定义相机故障字
 * @version 0.1
 * @date 2022-11
 */

#ifndef SRC_CAMERA_ERRORCODE_H_
#define SRC_CAMERA_ERRORCODE_H_

/**
 * @brief 相机工作状态
 */
enum {
	CAMERA_ERROR,
	CAMERA_IDLE,
	CAMERA_EXPOSE,
	CAMERA_IMGRDY
};

/**
 * @brief 相机故障字
 */
enum {
	CAMEC_SUCCESS,		// 无故障
	CAMEC_OFFLINE,		// 离线
	CAMEC_FAIL_INIT,	// 库初始化错误
	CAMEC_NOT_FOUND,	// 找不到相机或缺失配置文件
	CAMEC_NOT_OPEN,		// 不能打开或连接
	CAMEC_FAIL_EXPDUR,	// 设置曝光时间失败
	CAMEC_FAIL_EXPOSE,	// 启动曝光失败
	CAMEC_FAIL_READOUT,	// 读出异常
	CAMEC_GET_TEMP,		// 采集温度异常, 作为与相机通信的心跳机制
	CAMEC_MAX
};

#endif
