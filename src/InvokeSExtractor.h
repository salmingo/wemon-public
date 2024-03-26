/**
 * @file InvokeSExtractor.h 定义图像处理接口, 从图像中提取星像及其特征
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief
 * - 调用SExtractor完成处理流程
 *
 * @version 0.1
 * @date 2023-09-22
 *
 * © ARTD Group, NAOC
 *
 */
#ifndef INVOKESEXTRACTOR_H
#define INVOKESEXTRACTOR_H

#include "Parameter.h"
#include "xmFrame.h"
#include "xmStarLink.h"

class InvokeSExtractor
{
public:
    InvokeSExtractor();
    ~InvokeSExtractor();

// 数据类型
private:

// 成员变量
private:
	const Parameter* param_;	///< 配置参数
	bool prepared_;	///< 处理准备就绪
	bool running_;	///< 运行中标志

	string pathExe_;	///< SExtractor可执行文件路径
	string nameExe_;	///< SExtractor可执行文件名称

	xmStarLink* headLink_;	///< 链表首指针
	int starCount_;	///< 星像计数

	xmFrmPtr frame_;	///< 当前处理的图像帧指针
	double elong_;		///< 延展率均值
	double elongErr_;	///< 延展率误差

// 接口
public:
	/**
	 * @brief 检查是否在处理图像
	 * @return 是否正在执行处理过程
	 */
	bool IsRun() { return running_; }
	/**
	 * @brief 设置配置参数
	 * @param param 配置参数
	 * @return 处理前准备结果
	 */
	bool Prepare(const Parameter* param);
	/**
	 * @brief 处理图像, 提取图像内星像及其特征
	 * @param frame  图像帧指针
	 * @return
	 * 0 : 成功
	 * 1 : 找不到可执行程序
	 * 2 : 不能启动进程
	 * 3 : SEx调用/执行错误
	 * 4 : 星像数量不足, 无法完成定标等流程
	 * 5 : 图像质量差
	 */
	int DoIt(xmFrmPtr frame);

// 功能
private:
	/**
	 * @brief 扫描查找SExtractor可执行程序路径
	 * @return 程序查找结果
	 */
	bool scan_executor();
	/**
	 * @brief 生成SExtractor程序执行时的依赖文件
	 * @return 文件生成结果
	 */
	bool generate_configuration();
	/**
	 * @brief 生成SExtractor程序执行时的依赖文件: 图像处理
	 * @return 文件生成结果
	 */
	bool generate_default_sex();
	/**
	 * @brief 生成SExtractor程序执行时的依赖文件: 输出项
	 * @return 文件生成结果
	 */
	bool generate_default_param();
	/**
	 * @brief 生成SExtractor程序执行时的依赖文件: 卷积模版
	 * @return 文件生成结果
	 */
	bool generate_default_conv();
	/**
	 * @brief 生成SExtractor程序执行时的依赖文件: 面源
	 * @return 文件生成结果
	 */
	bool generate_default_nnw();

// 功能
private:
	/**
	 * @brief 读取SEx输出文件, 提取信息转存至xmFrame对象
	 * @param pathCat  SEx输出的星像特征文件
	 * @return
	 * 星像数量
	 */
	int resolve_catalog(const char* pathCat);
	/**
	 * @brief 统计评估图像质量
	 * @return 图像质量是否符合条件
	 * @note 质量标准:
	 * - 图像纵/横各4等分, 共16个区域, 每个区域至少包含3颗星
	 */
	bool stat_quality();
	/**
	 * @brief 统计评估拖长星像倾角
	 * @return true: 星像有统计倾角, 不需要统计FWHM
	 * @note 若大部分星像有明显倾角, 则FWHM<--0
	 */
	bool stat_incline();
	/**
	 * @brief 统计星像半高全宽
	 */
	void stat_fwhm();
	/**
	 * @brief 统计延展率
	 */
	bool stat_elong();
	/**
	 * @brief 剔除被污染的拖长星像
	 */
	void remove_polluted();
	/**
	 * @brief 将链表中存储的合法星像转存至图像帧中的星像集合
	 * @param frame  图像帧指针
	 */
	void link2frame();
};

#endif
