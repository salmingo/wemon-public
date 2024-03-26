
#ifndef _SRC_CAMERA_BASE_H_
#define _SRC_CAMERA_BASE_H_

#include <string>
#include <boost/signals2/signal.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include "BoostInclude.h"
#include "CameraDefine.h"

using std::string;
using boost::posix_time::ptime;

//=====================================================================
/**
 * @brief 相机工作状态
 */
struct CameraInfo {
	/* 控制量 */
	bool connected;	///< 连接标志
	int state;		///< 状态
	int errcode;	///< 故障字
	int errcnt;		///< 连续故障次数

	/* 静态量 */
	string model;	///< 型号名称
	uint32_t wSensor;	///< 感光区宽度, 像元
	uint32_t hSensor;	///< 感光区高度, 像元
	float pixSizeX;	///< 像元尺寸, X方向, 微米
	float pixSizeY;	///< 像元尺寸, Y方向, 微米

	/* 制冷 */
	bool coolOn;	///< 制冷启动标志
	int coolSet;	///< 制冷温度
	int coolGet;	///< 探测器温度

	/* A/D */
	uint16_t iADChannel;	///< AD通道
	uint16_t iReadport;		///< 读出端口
	uint16_t iReadrate;		///< 读出速度
	uint16_t iPreampGain;	///< 前置增益
	uint16_t iVerShift;		///< 行转移速度
	bool EMSupport;			///< 支持: EM功能

	uint16_t bitdepth;		///< 数字位宽
	string readport;		///< 读出端口名称
	string readrate;		///< 读出速度
	float gainPreamp;		///< 前置增益, e-/DU
	float verShiftRate;		///< 行转移速度
	bool EMON;				///< 启用EM
	uint16_t EMGain;		///< EM增益

	/* ROI */
	bool useROI;			///< ROI启用标志
	int xorgin, yorgin;		///< 在原始图像中的起点坐标. 原点: (1,1)
	int width, height;		///< 在原始图像中的宽度和高度
	int xbin, ybin;			///< 合并因子

	/* 快门 */
	bool hasShutter;	///< 有机械快门
	int shtrMode;		///< 快门模式. 0: 自动; 1: 常开; 2: 常关

	/* 曝光 */
	bool triggerExt;///< 使用外部触发
	bool capturing;	///< 在尝试采集图像. 用于外部触发模式
	double expdur;	///< 曝光时间, 量纲: 秒
	ptime dateobs;	///< 曝光开始时间, 微秒
	ptime dateend;	///< 曝光结束时间, 微秒

	/* 图像数据 */
	uint32_t pixels;	///< 像素数
	ArrayCharU data;	///< 数据存储区

public:
	CameraInfo() {
		Reset();
	}

	~CameraInfo() {
		data.reset();
	}

	void Reset() {
		connected = false;
		state   = CAMERA_ERROR;
		errcode = CAMEC_OFFLINE;
		errcnt  = 0;
		coolOn= false;
		capturing = false;
		expdur  = __DBL_MAX__;
		useROI  = false;
		pixels  = 0;
		data.reset();
	}

	/**
	 * @brief 为感光区图像数据分配内存空间
	 */
	void Alloc() {
		uint32_t szmem = pixels * (bitdepth <= 8 ? 1 : (bitdepth <= 16 ? 2 : 4));
		szmem = (szmem + 15) & ~15; // 16字节对齐
		data = make_shared_array<unsigned char>(szmem);
	}
};
//=====================================================================

class CameraBase {
public:
	/**
	 * @brief 曝光进度回调函数
	 * @param state   工作状态
	 * @param percent 曝光进度百分比
	 * @param left    剩余曝光时间
	 */
	typedef boost::signals2::signal<void (int state, double percent, double left)> CBF;
	typedef CBF::slot_type CBSlot;

protected:
	CBF cbfExpose_;		///< 曝光进度回调函数
	CameraInfo info_;	///< 相机配置及状态
	ThrdPtr thrdExpose_;///< 线程: 监测曝光过程
	ThrdPtr thrdTemp_;	///< 线程: 在空闲时监测芯片温度
	boost::condition_variable cvExpBegin_;	///< 事件: 开始曝光
	boost::condition_variable cvExpOver_;	///< 事件: 曝光结束

public:
	CameraBase();
	virtual ~CameraBase();
	const CameraInfo* GetInfo() {
		return &info_;
	}

public:
	/* 接口 */
	/**
	 * @brief 注册曝光进度回调函数
	 * @param slot  插槽函数
	 */
	void RegisterExpose(const CBSlot& slot);
	/**
	 * @brief 连接相机
	 * @return 与相机的连接结果
	 */
	bool Connect();
	/**
	 * @brief 断开与相机的连接
	 */
	void Disconnect();
	/**
	 * @brief 启动/停止制冷
	 * @param onoff     true: 启动制冷; false: 停止制冷
	 * @param coolerSet 制冷温度, 摄氏度
	 */
	void CoolerOnoff(bool onoff = true, int coolerSet = 0);
	/**
	 * @brief 开始曝光
	 * @param expdur  曝光时间, 秒
	 * @param light   是否需要天光
	 * @return 操作执行结果
	 */
	bool Expose(double expdur, bool light = true);
	/**
	 * @brief 中止曝光
	 * @return 操作执行结果
	 */
	bool AbortExpose();
	/**
	 * @brief 设置感兴趣窗口
	 * @param x0    X起始坐标
	 * @param y0    Y起始坐标
	 * @param w     窗口宽度
	 * @param h     窗口高度
	 * @param xbin  X合并因子
	 * @param ybin  Y合并因子
	 * @return 操作结果
	 */
	bool SetROI(int &x0, int &y0, int &w, int &h, int xbin = 1, int ybin = 1);
	/**
	 * @brief 设置AD通道
	 * @param index     档位索引
	 * @return 操作结果成功标志
	 */
	bool SetADChannel(uint16_t index);
	/**
	 * @brief 设置读出端口
	 * @param index  档位索引
	 * @return 操作结果成功标志
	 */
	bool SetReadPort(uint16_t index);
	/**
	 * @brief 设置读出速度
	 * @param index  档位索引
	 * @return 操作结果成功标志
	 */
	bool SetReadRate(uint16_t index);
	/**
	 * @brief 设置A/D增益
	 * @param index  档位索引
	 * @return 操作结果成功标志
	 */
	bool SetPreampGain(uint16_t index);
	/**
	 * @brief 设置行转移速度
	 * @param index  档位索引
	 * @return 操作结果成功标志
	 */
	bool SetVerticalShift(uint16_t index);
	/**
	 * @brief 启用/停用EM模式, 并设置EM增益
	 * @param onoff    启用标志
	 * @param gain     EM增益
	 * @return 操作结果成功标志
	 */
	bool SetEMGain(bool onoff = false, uint16_t gain = 1);

protected:
	/* 功能 */
	/**
	 * @brief 建立与相机的连接
	 * @return 连接结果
	 */
	virtual bool open_camera() = 0;
	/**
	 * @brief 断开与相机的连接
	 */
	virtual void close_camera() = 0;
	/**
	 * @brief 启动/停止制冷
	 * @param onoff      true: 启动; false: 停止
	 * @param coolerSet  制冷温度, 摄氏度
	 */
	virtual void cooler_onoff(bool onoff, int coolerSet) = 0;
	/**
	 * @brief 获得芯片温度
	 * @return 芯片温度, 摄氏度
	 * @note
	 * 温度采集错误时, 代表相机异常, 需要做故障诊断
	 */
	virtual bool sensor_temperature(int& temperature) = 0;
	/**
	 * @brief 设置快门模式
	 * @param mode   快门模式. 0: 自动; 1: 常开; 2: 常关
	 * @return 操作执行结果
	 */
	virtual bool set_ShtrMode(int mode);
	/**
	 * @brief 设置曝光时间
	 * @param expdur   曝光时间, 秒
	 * @return 操作执行结果
	 */
	virtual bool set_expdur(double expdur) = 0;
	/**
	 * @brief 启动曝光流程
	 * @return 操作执行结果
	 */
	virtual bool start_expose() = 0;
	/**
	 * @brief 停止曝光流程
	 * @return 操作执行结果
	 */
	virtual bool stop_expose() = 0;
	/**
	 * @brief 设置感兴趣窗口
	 */
	virtual bool set_ROI(int x0, int y0, int w, int h, int xbin, int ybin) = 0;
	/**
	 * @brief 设置AD通道
	 * @param index     档位索引
	 * @param bitdepth  像元位宽
	 * @return 操作结果成功标志
	 */
	virtual bool set_ADChannel(uint16_t index, uint16_t &bitdepth) = 0;
	/**
	 * @brief 设置读出端口
	 * @param index     档位索引
	 * @param value     读出端口名称
	 * @return 操作结果成功标志
	 */
	virtual bool set_ReadPort(uint16_t index, string& value) = 0;
	/**
	 * @brief 设置读出速度
	 * @param index     档位索引
	 * @param value     读出速度名称
	 * @return 操作结果成功标志
	 */
	virtual bool set_ReadRate(uint16_t index, string& value) = 0;
	/**
	 * @brief 设置前置增益
	 * @param index     档位索引
	 * @param gain      增益数值
	 * @return 操作结果成功标志
	 */
	virtual bool set_gain_preamp(uint16_t index, float& gain) = 0;
	/**
	 * @brief 设置行转移速度
	 * @param index     档位索引
	 * @param rate      速度值
	 * @return 操作结果成功标志
	 */
	virtual bool set_vershift(uint16_t index, float& rate) = 0;
	/**
	 * @brief 设置AD通道
	 * @param onoff     启用/停用EM模式
	 * @param gain      EM增益
	 * @return 操作结果成功标志
	 */
	virtual bool set_gain_em(bool onoff, uint16_t gain) = 0;
	/**
	 * @brief 从ROM中读取相机的可配置参数
	 */
	virtual bool init_parameters() = 0;
	/**
	 * @brief 从文件中加载相机的可配置参数
	 */
	virtual void load_parameters() = 0;

protected:
	/* 线程 */
	/**
	 * @brief 监测曝光过程
	 * @note 空闲时采集相机温度
	 */
	void thread_expose();
	/**
	 * @brief 线程: 监测探测器温度
	 */
	void thread_temperature();
};
typedef boost::shared_ptr<CameraBase> CameraPtr;

#endif
