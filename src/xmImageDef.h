/**
 * @file xmImageDef.h 定义在运动目标提取流程中使用的一些常量
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @version 0.1
 * @date 2023-04-05
 */

#ifndef XMIMAGEDEF_H_
#define XMIMAGEDEF_H_

// 星像提取/测量
#define ZONE_SIZE           512 // 图像坐标系划分天区的大小
#define ZONE_CROSS_SIZE		64	// 图像交叉窗口大小
#define STAR_COUNT_MIN      50  // 全图最少星数
#define STAR_AREA_MIN		3   // 星像最小面积
#define STAR_PER_ZONE       3   // 每个天区的最少星数
#define BAD_ZONE_MAX        3   // 坏天区最大值

// 星像模型
#define SHAPE_POINT         2   // 模型中星像数量(不含中心、定向)
#define SHAPE_VA_MAX        30  // 模型顶角上限, 角度
#define SHAPE_SIDE_MAX      1000/// 模型边长上限, 像元

#endif
