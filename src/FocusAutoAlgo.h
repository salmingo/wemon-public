/**
 * @file FocusAutoAlgo.h 定义云量相机自动调焦算法和接口
 * @author 卢晓猛 lxm@nao.cas.cn
 * @brief
 * - 初始化, 准备环境
 * - 通知新的焦点位置, 并获得调焦步长
 * - 结束
 * @version 0.1
 * @date 2023-10-27
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef FOCUS_AUTO_ALGO_H_
#define FOCUS_AUTO_ALGO_H_

#include <math.h>

struct FocusAutoAlgo {
	double expFwhm;	///< 期望FWHM
	double expFwhmErr;	///< 期望FWHM的误差
	double fwhmLast;	///< 最后一次的FWHM
	int    stepLast;	///< 最后一次调节步长

public:
	/**
	 * @brief 初始化调焦过程
	 * @param fwhm  期望FWHM
	 * @param err   期望FWHM的误差
	 */
	void Init(double fwhm, double err) {
		expFwhm    = fwhm;
		expFwhmErr = err;
		fwhmLast   = __DBL_MAX__;
	}

	/**
	 * @brief 设置当前FWHM
	 * @param fwhm  半高全宽
	 * @param step  调焦步长
	 * @return
	 * true- 结束调焦, 已达到理想像质
	 * false- 继续调焦, step中包含调焦步长
	 */
	bool Push(double fwhm, int &step) {
		if (fwhm <= (expFwhm + expFwhmErr)) return true;

		if (fwhmLast == __DBL_MAX__) {
			step = 500;
		}
		else {
			step = int((expFwhm - fwhm) * stepLast * 80 / (fwhm - fwhmLast)) / 100;
			if      (abs(step) > 5000) step = step > 5000 ? 5000 : -5000;
			else if (abs(step) > 2000) step = step > 2000 ? 2000 : -2000;
			else if (abs(step) > 500)  step = step > 500 ? 500 : -500;
			else if (abs(step) > 100)  step = step > 0 ? 100 : -100;
		}
		stepLast = step;
		fwhmLast = fwhm;

		return abs(step) < 100;
	}
};

#endif
