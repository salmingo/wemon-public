/**
 * @file SerialComm.cpp 定义文件, 基于boost::asio实现串口异步操作
 * @version 0.1
 * @date 2017-10-10
 */
#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "SerialComm.h"

#define SERIAL_BUFF_SIZE		128

using namespace boost::asio;
using namespace boost::placeholders;
using boost::system::error_code;

SerialComm::SerialComm()
	: port_(keep_.GetIOService()) {
	bufrcv_.reset(new char[SERIAL_BUFF_SIZE]);
	crcrcv_.set_capacity(SERIAL_BUFF_SIZE * 10);
	crcsnd_.set_capacity(SERIAL_BUFF_SIZE * 10);
	msglen_ = 0;
}

SerialComm::~SerialComm() {
	Close();
}

bool SerialComm::Open(const char* portname, const int baud_rate) {
	if (port_.is_open()) return true;
	error_code ec;
	port_.open(portname, ec);
	if (!ec) {
		port_.set_option(serial_port::baud_rate(baud_rate));
		port_.set_option(serial_port::stop_bits(serial_port::stop_bits::one));
		port_.set_option(serial_port::parity(serial_port::parity::none));
		port_.set_option(serial_port::character_size(8));
		start_read();
	}

	return !ec;
}

void SerialComm::Close() {
	if (port_.is_open()) {
		error_code ec;
		port_.close(ec);
	}
}

void SerialComm::SetReadLength(size_t length) {
	msglen_ = length;
}

bool SerialComm::IsOpen() {
	return port_.is_open();
}

int SerialComm::Lookup(const char* flag, const size_t len, const size_t from) {
	if (!flag || len <= 0 || from < 0) return -1;

	MtxLck lck(mtxrcv_);
	size_t n(crcrcv_.size() - len), pos, i(0), j;
	for (pos = from, i = -1; i != len && pos <= n; ++pos) {
		for (i = 0, j = pos; i < len && flag[i] == crcrcv_[j]; ++i, ++j);
	}
	return (i == len) ? (pos - 1) : -1;
}

int SerialComm::Write(const char* buff, const size_t len) {
	if (!buff || len <= 0 || !port_.is_open()) return 0;

	MtxLck lck(mtxsnd_);
	size_t n0(crcsnd_.size()), n(crcsnd_.capacity() - n0), i;

	if (n > len) n = len;
	for (i = 0; i < n; ++i) crcsnd_.push_back(buff[i]);
	if (!n0 && port_.is_open()) start_write();
	return n;
}

int SerialComm::Read(char* buff, const size_t len, const size_t from, bool erase) {
	if (!buff || len == 0) return 0;

	MtxLck lck(mtxrcv_);
	size_t end(from + len);
	size_t to_read = crcrcv_.size() > end ? len : crcrcv_.size() - from;

	if (to_read) {
		size_t i, j;
		for (i = 0, j = from; i < to_read; ++i, ++j) buff[i] = crcrcv_[j];
		if (erase) crcrcv_.erase_begin(end);
	}
	return to_read;
}

void SerialComm::RegisterRead(const CBSlot& slot) {
	MtxLck lck(mtxrcv_);
	if (!cbrcv_.empty()) cbrcv_.disconnect_all_slots();
	cbrcv_.connect(slot);
}

void SerialComm::RegisterWrite(const CBSlot& slot) {
	MtxLck lck(mtxsnd_);
	if (!cbsnd_.empty()) cbsnd_.disconnect_all_slots();
	cbsnd_.connect(slot);
}

void SerialComm::handle_read(const error_code& ec, size_t n) {
	if (!ec) {
		MtxLck lock(mtxrcv_);
		for(size_t i = 0; i < n; ++i) crcrcv_.push_back(bufrcv_[i]);
	}
	if (msglen_ == 0 || crcrcv_.size() >= msglen_)
		cbrcv_(this, ec.value(), crcrcv_.size());
	if (!ec) start_read();
}

void SerialComm::handle_write(const error_code& ec, size_t n) {
	if (!ec) {
		MtxLck lock(mtxsnd_);
		crcsnd_.erase_begin(n);
	}
	cbsnd_(this, ec.value(), n);
	if (!ec) start_write();
}

void SerialComm::start_read() {
	port_.async_read_some(buffer(bufrcv_.get(), SERIAL_BUFF_SIZE),
			boost::bind(&SerialComm::handle_read, this,
					placeholders::error, placeholders::bytes_transferred));
}

void SerialComm::start_write() {
	int n(crcsnd_.size());
	if (n) {
		port_.async_write_some(boost::asio::buffer(crcsnd_.linearize(), n),
				boost::bind(&SerialComm::handle_write, this,
						placeholders::error, placeholders::bytes_transferred));
	}
}
