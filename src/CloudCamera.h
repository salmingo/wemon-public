/**
 * @file CloudCamera.h 云量相机控制接口
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief 控制云量相机完成图像采集和文件存储
 * @version 0.1
 */
#ifndef CLOUDCAMERA_H
#define CLOUDCAMERA_H

#include <deque>
#include <boost/signals2/signal.hpp>
#include "Parameter.h"
#include "CameraBase.h"
#include "xmFrame.h"
#include "InvokeSExtractor.h"
#include "FocusAutoAlgo.h"
#include "AsioUDP.h"

enum {
	WMC_SUCCESS,	///< 正确
	WMC_FAIL_CONNECT,	///< 连接失败
	WMC_FAIL_READOUT	///< 读出失败
};

struct InfoCloudCamera {
	int state;	///< 状态
	string lastobs;	///< 最后一次采集图像的时间, UTC字符串
};

class CloudCamera
{
public:
    typedef boost::shared_ptr<CloudCamera> Pointer;
	/*!
	 * @brief 声明回调函数及插槽: 调焦量
	 * @param 1 0: 调焦量; 1: 调焦结束
	 * @param 2 调焦量或调焦结果
	 * @note
	 * - rslt == 0时, value=调焦量
	 * - rslt == 1时, value > 0 --> value=fwhm * 100
	 *   value<=0表示失败
	 */
	typedef boost::signals2::signal<void (const int rslt, const int16_t value)> CBF;
	typedef CBF::slot_type CBSlot;

public:
    CloudCamera(const Parameter* param);
    ~CloudCamera();
    static Pointer Create(const Parameter* param) {
        return Pointer(new CloudCamera(param));
    }

public:
	const InfoCloudCamera* GetInfo() {
		return &info_;
	}
    /**
     * @brief 启动云图采集服务
     * @return 服务启动结果
     */
    bool Start();
    /**
     * @brief 停止云图采集服务
     */
    void Stop();
	/**
	 * @brief 注册调焦回调函数
	 */
	void RegisterCBFocus(const CBSlot& slot);
	/**
	 * @brief 启动/停止调焦流程
	 * @param enable  启动标志
	 * @param manual  手动调焦
	 */
	void DoFocus(UdpPtr udp, bool enable = false, bool manual = true);
	/**
	 * @brief 最后一次调焦命令步长超出限位区域
	 */
	void FocusTargetOverLimit();
	/**
	 * @brief 命令调焦转到指定步长
	 * @param step  > 0: 顺时针; < 0: 逆时针
	 */
	void FocusMove(int step);

private:
	/* 功能 */
	/**
	 * @brief 回调函数: 相机曝光进度
	 * @param state    相机状态
	 * @param percent  曝光进度
	 * @param left     剩余时间
	 */
	void expose_process(int state, double percent, double left);
	/**
	 * @brief 将云图保存为FITS文件
	 * @return
	 * 0    -- 成功
	 * 其它 -- 错误代码
	 */
	int cloud2fits();
	/**
	 * @brief 依据图像中心统计结果, 修正曝光时间
	 */
	void cloudadj();

private:
	/**
	 * @brief 线程: 监测云量相机工作进度和状态
	 */
	void run();
	/**
	 * @brief 线程: 处理图像, 统计半高全宽
	 */
	void thread_reduce();

// 数据类型
private:
	typedef struct xmFrameQueue {
		boost::mutex mtx;
		std::deque<xmFrmPtr> frames;

	public:
		void Push(xmFrmPtr frame) {
			MtxLck lck(mtx);
			frames.push_back(frame);
		}

		xmFrmPtr Pop() {
			xmFrmPtr frame;
			if (frames.size()) {
				MtxLck lck(mtx);
				frame = frames.front();
				frames.pop_front();
			}
			return frame;
		}

		bool Empty() {
			return frames.empty();
		}
	} xmFrmQue;
	typedef std::deque<double> dblQue;

private:
	const Parameter* param_; ///< 配置参数
	InfoCloudCamera info_;	///< 云量相机状态信息
	CameraPtr camPtr_;	///< 相机接口
	int expdur_;		///< 云量相机曝光时间
	int frmno_;			///< 帧序号
    FILE* fpLog_;       ///< 日志文件

    string dirRawImg_;      ///< 原始图像文件存储目录
	string pathNtfyProc_;	///< 向处理软件告知图像文件

    ThrdPtr thrdMain_;  ///< 主线程

	/* 调焦 */
	int focusMode_;	///< 调焦模式. 0- 停止; 1- 手动; 2- 自动
	InvokeSExtractor invSEx_;	///< SExtractor接口
	xmFrmQue queImg_;	///< 图像帧队列
	dblQue   queFwhm_;	///< FWHM队列
	ThrdPtr thrdReduce_;	///< 线程: 数据处理
	boost::condition_variable cvNewImg_;	///< 有新的图像
	// 自动调焦控制量
	CBF cbfFocus_;	///< 回调函数: 调焦过程和结果
	// 自动调焦接口
	FocusAutoAlgo focusAlgo_;	///< 算法流程接口
	UdpPtr udpFocusPtr_;	///< 调焦网络通信接口
};
typedef CloudCamera::Pointer CloudCamPtr;

#endif
