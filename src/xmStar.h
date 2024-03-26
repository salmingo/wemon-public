/**
 * @file xmStar.h 定义星像特征
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief
 * 正常星像定义:
 * - 点像: 恒星, 能量分布基本符合高斯分布
 * - 拖长星像: 拖长方向近似均匀分布, 垂直方向基本符合高斯分布
 * - 面像: 面源(星云星团等), 不确定几何形状
 *
 * @version 0.1
 * @date 2023-09-18
 *
 * © ARTD Group, NAOC
 *
 */

#ifndef XM_STAR_H_
#define XM_STAR_H_

#include <vector>
#include "BoostInclude.h"

/**
 * @brief 单星像特征集合
 */
struct xmStar {
	/* 几何特征 */
	double x, y;	///< 质心坐标
	double elong;	///< 延展率
	int area;		///< 构成星像的像素数量
	double theta;	///< 半长轴/长度方向相对X轴正向的倾角, 量纲: 角度
	double fwhm;	///< 半高全宽

	/* 亮度特征 */
	double flux;	///< 流量
	double fluxErr;	///< 流量误差
	double fluxMax;	///< 峰值流量
	double mag;		///< 仪器星等
	double magErr;	///< 星等误差
	double snr;		///< 信噪比

	/* 统计条件 */
	bool inStat;	///< 是否符合统计条件
	bool refstar;	///< 可作为参考星参与定位和定标, 适用于拖长星像

	/* 天体特征 */
	int matched;	///< 匹配结果. 0: 无匹配; 1: 星表匹配; 2: 相邻帧匹配
	double raCat, decCat;	///< 星表坐标. J2000, 已修正自行
	double magCat;	///< 星表星等
	double raFit, decFit;	///< 定位坐标, J2000. 角度
	double magFit;			///< 定标星等

public:
	typedef boost::shared_ptr<xmStar> Pointer;

	xmStar() = default;
	static Pointer Create() {
		return Pointer(new xmStar);
	}
};

typedef xmStar::Pointer xmStarPtr;	///< 星像特征指针
typedef std::vector<xmStar> xmStarVec; ///< 星像特征数组
typedef std::vector<xmStarPtr> xmStarPtrVec;	///< 指针型星像特征数组

#endif
