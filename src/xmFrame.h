/**
 * @file xmFrame.h 定义图像帧特征
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief
 * -
 * @version 0.1
 * @date 2023-09-19
 *
 * © ARTD Group, NAOC
 *
 */

#ifndef XM_FRAME_H_
#define XM_FRAME_H_

#include <string>
#include <deque>
#include "xmStar.h"

using std::string;

struct xmFrame {
	// 文件名和路径
	string fileName;	///< 文件名
	string dirName;		///< 目录名
	string filePath;	///< 路径名

	// 图像分辨率
	int width;		///< 宽度
	int height;		///< 高度

	// 时间
	string dateObs;		///< 曝光开始时间, CCYY-MM-DDThh:mm:ss.ssssss
	double expTime;		///< 曝光时间

	// 图像处理结果
	bool bTrailing;		///< 星像统计拖尾
	/*! 图像处理结果统计标志
	 *  0 = 质量优, 可继续定位定标关联
	 *  1 = 像质差, 需检查光学系统或人工调焦
	 * ....待补充
	 */
	int flag;
	double back;		///< 全图统计背景亮度
	double incl;		///< 统计倾角, 量纲: 角度
	double inclErr;		///< 倾角统计误差, 量纲: 角度
	double fwhm;		///< 统计半高全宽. 0 == 无效
	double fwhmErr;		///< 统计半高全宽误差

	// 定位结果
	bool astroFix;		///< 天文/轴系定位结果
	double ra0, dec0;	///< 图像中心坐标, J2000
	//...WCS
	double raErr, decErr;	///< 天文定位残差, 量纲: 角秒

	// 测光结果
	bool photoFix;		///< 流量定标结果

	// 图像内提取星像集合
	xmStarPtrVec stars;	///< 星像集合

// 构造
public:
	typedef boost::shared_ptr<xmFrame> Pointer;

	xmFrame() = default;
	static Pointer Create() {
		return Pointer(new xmFrame);
	}

// 接口
public:
	/**
	 * @brief 设置要处理的FITS图像文件, 并尝试完成初始化操作
	 * @param pathImageFile  FITS图像文件路径
	 * @return 文件初始化结果
	 */
	bool Reset(const string& pathImageFile);
};

typedef xmFrame::Pointer xmFrmPtr;	///< 星像帧指针

#endif
