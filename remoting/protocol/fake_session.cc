// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";

FakeSocket::FakeSocket()
    : async_write_(false),
      write_pending_(false),
      write_limit_(0),
      next_write_error_(net::OK),
      next_read_error_(net::OK),
      read_pending_(false),
      read_buffer_size_(0),
      input_pos_(0),
      message_loop_(base::MessageLoop::current()),
      weak_factory_(this) {
}

FakeSocket::~FakeSocket() {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
}

void FakeSocket::AppendInputData(const std::vector<char>& data) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  input_data_.insert(input_data_.end(), data.begin(), data.end());
  // Complete pending read if any.
  if (read_pending_) {
    read_pending_ = false;
    int result = std::min(read_buffer_size_,
                          static_cast<int>(input_data_.size() - input_pos_));
    CHECK(result > 0);
    memcpy(read_buffer_->data(),
           &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    read_buffer_ = NULL;
    read_callback_.Run(result);
  }
}

void FakeSocket::PairWith(FakeSocket* peer_socket) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  peer_socket_ = peer_socket->weak_factory_.GetWeakPtr();
  peer_socket->peer_socket_ = weak_factory_.GetWeakPtr();
}

int FakeSocket::Read(net::IOBuffer* buf, int buf_len,
                     const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());

  if (next_read_error_ != net::OK) {
    int r = next_read_error_;
    next_read_error_ = net::OK;
    return r;
  }

  if (input_pos_ < static_cast<int>(input_data_.size())) {
    int result = std::min(buf_len,
                          static_cast<int>(input_data_.size()) - input_pos_);
    memcpy(buf->data(), &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    return result;
  } else {
    read_pending_ = true;
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeSocket::Write(net::IOBuffer* buf, int buf_len,
                      const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  EXPECT_FALSE(write_pending_);

  if (write_limit_ > 0)
    buf_len = std::min(write_limit_, buf_len);

  if (async_write_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FakeSocket::DoAsyncWrite, weak_factory_.GetWeakPtr(),
        scoped_refptr<net::IOBuffer>(buf), buf_len, callback));
    write_pending_ = true;
    return net::ERR_IO_PENDING;
  } else {
    if (next_write_error_ != net::OK) {
      int r = next_write_error_;
      next_write_error_ = net::OK;
      return r;
    }

    DoWrite(buf, buf_len);
    return buf_len;
  }
}

void FakeSocket::DoAsyncWrite(scoped_refptr<net::IOBuffer> buf, int buf_len,
                              const net::CompletionCallback& callback) {
  write_pending_ = false;

  if (next_write_error_ != net::OK) {
    int r = next_write_error_;
    next_write_error_ = net::OK;
    callback.Run(r);
    return;
  }

  DoWrite(buf.get(), buf_len);
  callback.Run(buf_len);
}

void FakeSocket::DoWrite(net::IOBuffer* buf, int buf_len) {
  written_data_.insert(written_data_.end(),
                       buf->data(), buf->data() + buf_len);

  if (peer_socket_.get()) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&FakeSocket::AppendInputData,
                   peer_socket_,
                   std::vector<char>(buf->data(), buf->data() + buf_len)));
  }
}

int FakeSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeSocket::Connect(const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  return net::OK;
}

void FakeSocket::Disconnect() {
  peer_socket_.reset();
}

bool FakeSocket::IsConnected() const {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  return true;
}

bool FakeSocket::IsConnectedAndIdle() const {
  NOTIMPLEMENTED();
  return false;
}

int FakeSocket::GetPeerAddress(net::IPEndPoint* address) const {
  net::IPAddressNumber ip(net::kIPv4AddressSize);
  *address = net::IPEndPoint(ip, 0);
  return net::OK;
}

int FakeSocket::GetLocalAddress(net::IPEndPoint* address) const {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

const net::BoundNetLog& FakeSocket::NetLog() const {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  return net_log_;
}

void FakeSocket::SetSubresourceSpeculation() {
  NOTIMPLEMENTED();
}

void FakeSocket::SetOmniboxSpeculation() {
  NOTIMPLEMENTED();
}

bool FakeSocket::WasEverUsed() const {
  NOTIMPLEMENTED();
  return true;
}

bool FakeSocket::UsingTCPFastOpen() const {
  NOTIMPLEMENTED();
  return true;
}

bool FakeSocket::WasNpnNegotiated() const {
  return false;
}

net::NextProto FakeSocket::GetNegotiatedProtocol() const {
  NOTIMPLEMENTED();
  return net::kProtoUnknown;
}

bool FakeSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

FakeUdpSocket::FakeUdpSocket()
    : read_pending_(false),
      input_pos_(0),
      message_loop_(base::MessageLoop::current()) {
}

FakeUdpSocket::~FakeUdpSocket() {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
}

void FakeUdpSocket::AppendInputPacket(const char* data, int data_size) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  input_packets_.push_back(std::string());
  input_packets_.back().assign(data, data + data_size);

  // Complete pending read if any.
  if (read_pending_) {
    read_pending_ = false;
    int result = std::min(data_size, read_buffer_size_);
    memcpy(read_buffer_->data(), data, result);
    input_pos_ = input_packets_.size();
    read_callback_.Run(result);
    read_buffer_ = NULL;
  }
}

int FakeUdpSocket::Read(net::IOBuffer* buf, int buf_len,
                        const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  if (input_pos_ < static_cast<int>(input_packets_.size())) {
    int result = std::min(
        buf_len, static_cast<int>(input_packets_[input_pos_].size()));
    memcpy(buf->data(), &(*input_packets_[input_pos_].begin()), result);
    ++input_pos_;
    return result;
  } else {
    read_pending_ = true;
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeUdpSocket::Write(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, base::MessageLoop::current());
  written_packets_.push_back(std::string());
  written_packets_.back().assign(buf->data(), buf->data() + buf_len);
  return buf_len;
}

int FakeUdpSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeUdpSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

FakeSession::FakeSession()
    : event_handler_(NULL),
      candidate_config_(CandidateSessionConfig::CreateDefault()),
      config_(SessionConfig::ForTest()),
      message_loop_(base::MessageLoop::current()),
      async_creation_(false),
      jid_(kTestJid),
      error_(OK),
      closed_(false),
      weak_factory_(this) {
}

FakeSession::~FakeSession() { }

FakeSocket* FakeSession::GetStreamChannel(const std::string& name) {
  return stream_channels_[name];
}

FakeUdpSocket* FakeSession::GetDatagramChannel(const std::string& name) {
  return datagram_channels_[name];
}

void FakeSession::SetEventHandler(EventHandler* event_handler) {
  event_handler_ = event_handler;
}

ErrorCode FakeSession::error() {
  return error_;
}

const std::string& FakeSession::jid() {
  return jid_;
}

const CandidateSessionConfig* FakeSession::candidate_config() {
  return candidate_config_.get();
}

const SessionConfig& FakeSession::config() {
  return config_;
}

void FakeSession::set_config(const SessionConfig& config) {
  config_ = config;
}

ChannelFactory* FakeSession::GetTransportChannelFactory() {
  return this;
}

ChannelFactory* FakeSession::GetMultiplexedChannelFactory() {
  return this;
}

void FakeSession::Close() {
  closed_ = true;
}

void FakeSession::CreateStreamChannel(
    const std::string& name,
    const StreamChannelCallback& callback) {
  scoped_ptr<FakeSocket> channel;
  // If we are in the error state then we put NULL in the channels list, so that
  // NotifyStreamChannelCallback() still calls the callback.
  if (error_ == OK)
    channel.reset(new FakeSocket());
  stream_channels_[name] = channel.release();

  if (async_creation_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FakeSession::NotifyStreamChannelCallback, weak_factory_.GetWeakPtr(),
        name, callback));
  } else {
    NotifyStreamChannelCallback(name, callback);
  }
}

void FakeSession::NotifyStreamChannelCallback(
    const std::string& name,
    const StreamChannelCallback& callback) {
  if (stream_channels_.find(name) != stream_channels_.end())
    callback.Run(scoped_ptr<net::StreamSocket>(stream_channels_[name]));
}

void FakeSession::CreateDatagramChannel(
    const std::string& name,
    const DatagramChannelCallback& callback) {
  scoped_ptr<FakeUdpSocket> channel;
  // If we are in the error state then we put NULL in the channels list, so that
  // NotifyStreamChannelCallback() still calls the callback.
  if (error_ == OK)
    channel.reset(new FakeUdpSocket());
  datagram_channels_[name] = channel.release();

  if (async_creation_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FakeSession::NotifyDatagramChannelCallback, weak_factory_.GetWeakPtr(),
        name, callback));
  } else {
    NotifyDatagramChannelCallback(name, callback);
  }
}

void FakeSession::NotifyDatagramChannelCallback(
    const std::string& name,
    const DatagramChannelCallback& callback) {
  if (datagram_channels_.find(name) != datagram_channels_.end())
    callback.Run(scoped_ptr<net::Socket>(datagram_channels_[name]));
}

void FakeSession::CancelChannelCreation(const std::string& name) {
  stream_channels_.erase(name);
  datagram_channels_.erase(name);
}

}  // namespace protocol
}  // namespace remoting
