/**
 * @file AsioUDP.cpp 基于boost::asio封装UDP通信, 定义文件
 * @version 1.1
 * @date 2023-11-03
 */

#ifdef _WINDOWS
#include "StdAfx.h"
#endif

#include <boost/system/system_error.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "AsioUDP.h"

using namespace boost;
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::placeholders;

UdpSession::UdpSession()
	: sock_(keep_.GetIOService()) {
	connected_ = false;
	blockRead_ = false;
	buffPack_ = make_shared_array<char>(UDP_PACK_SIZE);
}

UdpSession::~UdpSession() {
	Close();
}

//////////////////////////////////////////////////////////////////////////////
void UdpSession::RegisterReceive(const CBSlot& slot) {
	cbfRcv_.disconnect_all_slots();
	cbfRcv_.connect(slot);
}

const char *UdpSession::WhatError(int *errCode) {
	if (errCode) *errCode = errCode_;
	return errDesc_.c_str();
}

//////////////////////////////////////////////////////////////////////////////
bool UdpSession::Open(uint16_t port, const char* ip, bool v6) {
	try {
		if (!keep_.IsKeeping()) keep_.Reset();
		if (!sock_.is_open()) sock_.open(v6 ? BoostUdp::v6() : BoostUdp::v4());
		sock_.set_option(socket_base::reuse_address(true));
		if (port) {
			if (ip) sock_.bind(BoostUdp::endpoint(ip::address::from_string(ip), port));
			else    sock_.bind(BoostUdp::endpoint(v6 ? BoostUdp::v6() : BoostUdp::v4(), port));
		}
		start_read();
		return true;
	}
	catch(system_error& ex) {
		errCode_ = ex.code().value();
		errDesc_ = ex.what();
		return false;
	}
}

bool UdpSession::Close() {
	try {
		if (sock_.is_open()) {
			keep_.Stop();
			sock_.close();
			connected_ = false;
		}
		return true;
	}
	catch(system_error& ex) {
		errCode_ = ex.code().value();
		errDesc_ = ex.what();
		return false;
	}
}

bool UdpSession::UseMulticast(const char* ip) {
	try {
		sock_.set_option(ip::multicast::join_group(ip::address::from_string(ip)));
		return true;
	}
	catch(system_error& ex) {
		errCode_ = ex.code().value();
		errDesc_ = ex.what();
		return false;
	}
}

bool UdpSession::Connect(const char* ipPeer, uint16_t port) {
	try {
		if (!connected_) {
			sock_.connect(BoostUdp::endpoint(ip::address::from_string(ipPeer), port));
			connected_ = true;
		}
	}
	catch(system_error& ex) {
		errCode_ = ex.code().value();
		errDesc_ = ex.what();
		return false;
	}
	return true;
}

BoostUdpSock& UdpSession::GetSocket() {
	return sock_;
}

const char *UdpSession::BlockRead(const void *data, int bytesWrite, int &bytesRead)
{
	boost::mutex mtx;
	MtxLck lck(mtx);

	blockRead_ = true;
	bytesRead  = 0;
	if (Write(data, bytesWrite)
		&& cvRcv_.wait_for(lck, boost::chrono::milliseconds(100)) == cv_status::no_timeout) {
		bytesRead = bytesRcv_;
	}
	blockRead_ = false;
	return bytesRead ? buffPack_.get() : NULL;
}

/**
 * @brief 阻塞读出已收到的信息
 * @param bytes  信息长度
 * @return 已收到信息地址
 */
const char *UdpSession::BlockRead(const void *data, int bytesWrite,
	const char *ipPeer, uint16_t port, int &bytesRead) {
	boost::mutex mtx;
	MtxLck lck(mtx);

	blockRead_ = true;
	bytesRead  = 0;
	if (WriteTo(data, bytesWrite, ipPeer, port)
		&& cvRcv_.wait_for(lck, boost::chrono::milliseconds(100)) == cv_status::no_timeout) {
		bytesRead = bytesRcv_;
	}
	blockRead_ = false;
	return bytesRead ? buffPack_.get() : NULL;
}

int UdpSession::Write(const void* data, const int toWrite) {
	int written(0);
	try {
		MtxLck lck(mtxWrite_);
		sock_.wait(BoostUdpSock::wait_write);
		written = connected_ ? sock_.send(buffer(data, toWrite))
			: sock_.send_to(buffer(data, toWrite), remote_);
	}
	catch(system_error& ex) {
		errCode_ = ex.code().value();
		errDesc_ = ex.what();
	}
	return written;
}

int UdpSession::WriteTo(const void* data, const int toWrite, const char* ipPeer, const uint16_t port) {
	int written(0);
	try {
		MtxLck lck(mtxWrite_);
		sock_.wait(BoostUdpSock::wait_write);
		written = sock_.send_to(buffer(data, toWrite),
			BoostUdp::endpoint(ip::address::from_string(ipPeer), port));
	}
	catch(system_error& ex) {
		errCode_ = ex.code().value();
		errDesc_ = ex.what();
	}
	return written;
}

void UdpSession::start_read() {
	if (connected_) {
		sock_.async_receive(buffer(buffPack_.get(), UDP_PACK_SIZE),
			bind(&UdpSession::handle_read, this,
				asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
	else {
		sock_.async_receive_from(buffer(buffPack_.get(), UDP_PACK_SIZE), remote_,
			bind(&UdpSession::handle_read, this,
				asio::placeholders::error, asio::placeholders::bytes_transferred));
	}
}

void UdpSession::handle_read(const error_code& ec, const int bytes) {
	if (!ec || ec == error::message_size) {
		buffPack_[bytes] = 0;
		bytesRcv_ = bytes;
		if (blockRead_) cvRcv_.notify_one();
		else cbfRcv_(buffPack_.get(), bytes);
	}
	start_read();
}
