/**
 * @Author Ives
 * @Date 2023/10/14 01:30
 * @Version : 1.0.0
 * @Contact : rszhang@bao.ac.cn
 * @License : Copyright (C), 2023 NAOC
 * @Desc : 定义云量相机中电调焦相关网络通信协议, 执行软件 <--> wemon.
 * - wemon : 依据FWHM, 计算调焦量 == 电机步进方向和步长. + : 电机顺时针; - : 电机逆时针
 * - 执行软件: 向USB串口发送步进指令
 *
 */

#ifndef CLOUDFOCUS_PROTOFOC_H
#define CLOUDFOCUS_PROTOFOC_H

#include <stdint.h>

#define	FOCUS_CHECK_CODE	0xFEDCBA98	///< 校验码

#define TYPE_FOCUS_BEGIN        0x1   ///< 开始调焦标志. 执行软件 --> wemon: 命令启动调焦
#define TYPE_FOCUS_END          0x2   ///< 结束调焦标志. 执行软件 --> wemon: 命令结束调焦; wemon --> 执行软件: 通知调焦已结束
#define TYPE_FOCUS_MOVE         0x3   ///< 调焦. + : 顺时针; - : 逆时针. 数值是步长. wemon --> 执行软件
#define TYPE_FOCUS_LIMIT        0x4   ///< 限位标志. wemon最后一次发送的步长超出限位范围. 执行软件 --> wemon

struct ProtoFocusBase {
    uint8_t type;
    uint32_t check;

public:
    ProtoFocusBase() {
        type  = 0;
        check = FOCUS_CHECK_CODE;
    }
};

/**
 * 发送开始调焦标志到weamon
 */
struct ProtoFocusBegin : public ProtoFocusBase {
	uint8_t manual;	///< 1: 手动调焦; 0: 自动调焦

public:
    ProtoFocusBegin() {
        type = TYPE_FOCUS_BEGIN;
		manual = 1;
    }
};

/**
 * wemon发送结束调焦标志到 focus，focus收到后退出程序
 */
struct ProtoFocusEnd : public ProtoFocusBase {
    int8_t    success;    ///< 1: 成功; 0: 失败
    uint16_t  fwhm;       ///< 若success==1, 则fwhm是调焦后的FWHM, 量纲: 0.01pixel

public:
    ProtoFocusEnd() {
        type = TYPE_FOCUS_END;
    }
};

/**
 * wemon发送调焦指令到focus
 */
struct ProtoFocusMove : public ProtoFocusBase {
    int32_t step;   ///< 转动步长和方向

public:
    ProtoFocusMove() {
        type = TYPE_FOCUS_MOVE;
    }
};

/*
 * 发送限位指令，当wemon的调焦指令可能会触发限位，发送这条指令。
 */
struct ProtoFocusLimit : public ProtoFocusBase {
public:
    ProtoFocusLimit() {
        type = TYPE_FOCUS_LIMIT;
    }
};

#endif //CLOUDFOCUS_PROTOFOC_H
