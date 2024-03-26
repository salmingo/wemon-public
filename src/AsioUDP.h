/*!
 * @file AsioUDP.h 基于boost::asio封装UDP通信, 声明文件
 * @note
 * - 基于标准C类库创建项目
 * @version 0.1
 * @date May 23, 2017
 *
 * @note
 * - 基于boost::asio重新封装
 * @version 0.2
 * @date Oct 30, 2020
 *
 * @note 优化
 * - 添加组播功能
 * - 优化代码结构
 * @version 0.3
 * @date 2022-9-21
 *
 * @note 优化
 * @date 2023-04-22
 * @note
 * 删除缓冲区
 *
 * @note 优化
 * @date 2023-10-15
 * @version 1.0
 * @note
 * - 删除shared_from_this. 该模式仅允许使用智能指针型实例
 * - 删除Read(), 使用回调函数处理收到的信息
 * - 合并Open()
 * - 在Write()和WriteTo()中, 使用wait替代sleep
 * - 优化Open()和Connect()
 *
 * @version 1.1
 * @date 2023-11-03
 * - 删除回调函数的第三个形参. 原第三个对应实例指针
 * - 更新BlockRead形参顺序
 */

#ifndef ASIOUDP_H_
#define ASIOUDP_H_

#include <boost/signals2/signal.hpp>
#include <boost/asio/ip/udp.hpp>
#include <string>
#include "BoostInclude.h"
#include "BoostAsioKeep.h"

using std::string;

//=====================================================================
typedef boost::asio::ip::udp	BoostUdp;		// boost::ip::udp
typedef BoostUdp::socket		BoostUdpSock;	// boost::ip::udp::socket
#define UDP_PACK_SIZE		1500
//=====================================================================

class UdpSession {
public:
	typedef boost::shared_ptr<UdpSession> Pointer;
	/*!
	 * @brief 声明回调函数及插槽
	 * @param 1 接收到的信息存储区地址
	 * @param 2 接收到的信息长度
	 */
	typedef boost::signals2::signal<void (const char* rcvd, int bytes)> CBF;
	typedef CBF::slot_type CBSlot;

public:
	UdpSession();
	virtual ~UdpSession();
	static Pointer Create() {
		return Pointer(new UdpSession);
	}

// 成员变量
protected:
	// 套接口和连接标志
	BoostAsioKeep keep_;		//< 提供boost::asio::io_service对象, 并在实例存在期间保持其运行
	BoostUdpSock sock_;			//< 套接口
	BoostUdp::endpoint remote_;	//< 远程套接口
	bool connected_;			///< 面向连接的UDP

	// 接收和发送
	int bytesRcv_;			///< 已收到信息长度
	ArrayChar buffPack_;	///< 单帧缓冲区
	boost::mutex mtxWrite_;	///< 互斥锁: 写入操作

	// 阻塞式功能
	bool blockRead_;	///< 阻塞式读出模式
	CBF cbfRcv_;	///< 回调函数: 接收
	boost::condition_variable cvRcv_;	///< 条件: 已收到反馈信息

	// 故障描述
	string errDesc_;	///< 故障描述
	int errCode_;		///< 故障码

public:
	/* 接口 */
	/**
	 * @brief 注册回调函数: 接收到信息
	 * @param slot  插槽函数
	 */
	void RegisterReceive(const CBSlot& slot);
	/**
	 * @brief 当发生错误时, 查看错误描述
	 * @param[out] errCode 故障码
	 * @return 错误描述
	 */
	const char* WhatError(int *errCode = NULL);

public:
	/**
	 * @brief 在IP和端口上建立UDP服务
	 * @param ip    IP地址
	 * @param port  端口, >0
	 * @param v6    是否使用IPv6
	 * @return 操作结果
	 */
	bool Open(uint16_t port = 0, const char* ip = NULL, bool v6 = false);
	/**
	 * @brief 关闭套接字
	 */
	bool Close();
	/**
	 * @brief 使用组播模式
	 * @param ip  组播地址, 有效范围: 224.0.2.0～238.255.255.255
	 */
	bool UseMulticast(const char *ip);
	/**
	 * @brief 连接UDP服务器
	 * @param ipPeer  远程主机地址
	 * @param port    远程主机端口
	 * @return 操作结果
	 */
	bool Connect(const char* ipPeer, uint16_t port);
	/**
	 * @brief 查看套接字
	 * @return UDP套接字
	 */
	BoostUdpSock& GetSocket();
	/**
	 * @brief 发送信息后等待反馈, 面向连接的UDP
	 * @param[in]  data        待发送信息
	 * @param[in]  bytesWrite  待发送信息长度
	 * @param[out] bytesRead   接收到的反馈信息长度
	 * @return 已收到反馈信息
	 */
	const char *BlockRead(const void *data, int bytesWrite, int &bytesRead);
	/**
	 * @brief 发送信息后等待反馈, 无连接的UDP
	 * @param[in]  data       待发送信息
	 * @param[in]  bytesWrite 待发送信息长度
	 * @param[in]  ipPeer     远程主机IP地址
	 * @param[in]  port       远程主机端口
	 * @param[out] bytesRead  接收到的反馈信息长度
	 * @return 已收到反馈信息
	 */
	const char *BlockRead(const void *data, int bytesWrite, const char *ipPeer, uint16_t port, int &bytesRead);
	/**
	 * @brief 发送数据: 面向连接UDP
	 * @param data        待发送数据
	 * @param bytesWrite  待发送数据长度
	 * @return 实际发送数据长度
	 */
	int Write(const void* data, int bytesWrite);
	/**
	 * @brief 发送数据: 无连接
	 * @param data        待发送数据
	 * @param bytesWrite  待发送数据长度
	 * @param ipPeer      主机地址
	 * @param port        主机端口
	 * @return 实际发送数据长度
	 */
	int WriteTo(const void* data, int bytesWrite, const char* ipPeer, uint16_t port);

protected:
	/* 功能 */
	/**
	 * @brief 异步读取收到的信息
	 */
	void start_read();
	/**
	 * @brief 异步处理收到的信息
	 * @param ec     故障字. 0: 无故障
	 * @param bytes  接收信息的长度, 字节
	 */
	void handle_read(const boost::system::error_code& ec, int bytes);
};
typedef UdpSession::Pointer UdpPtr;

#endif
