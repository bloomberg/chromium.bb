// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";

FakeSocket::FakeSocket()
    : read_pending_(false),
      read_buffer_size_(0),
      input_pos_(0),
      message_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

FakeSocket::~FakeSocket() {
  EXPECT_EQ(message_loop_, MessageLoop::current());
}

void FakeSocket::AppendInputData(const std::vector<char>& data) {
  EXPECT_EQ(message_loop_, MessageLoop::current());
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
  EXPECT_EQ(message_loop_, MessageLoop::current());
  peer_socket_ = peer_socket->weak_factory_.GetWeakPtr();
  peer_socket->peer_socket_ = weak_factory_.GetWeakPtr();
}

int FakeSocket::Read(net::IOBuffer* buf, int buf_len,
                     const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, MessageLoop::current());
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
  EXPECT_EQ(message_loop_, MessageLoop::current());
  written_data_.insert(written_data_.end(),
                       buf->data(), buf->data() + buf_len);

  if (peer_socket_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FakeSocket::AppendInputData, peer_socket_,
        std::vector<char>(buf->data(), buf->data() + buf_len)));
  }

  return buf_len;
}

bool FakeSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}
bool FakeSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

int FakeSocket::Connect(const net::CompletionCallback& callback) {
  EXPECT_EQ(message_loop_, MessageLoop::current());
  return net::OK;
}

void FakeSocket::Disconnect() {
  peer_socket_.reset();
}

bool FakeSocket::IsConnected() const {
  EXPECT_EQ(message_loop_, MessageLoop::current());
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
  return net::ERR_FAILED;
}

const net::BoundNetLog& FakeSocket::NetLog() const {
  EXPECT_EQ(message_loop_, MessageLoop::current());
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

int64 FakeSocket::NumBytesRead() const {
  NOTIMPLEMENTED();
  return 0;
}

base::TimeDelta FakeSocket::GetConnectTimeMicros() const {
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

net::NextProto FakeSocket::GetNegotiatedProtocol() const {
  NOTIMPLEMENTED();
  return net::kProtoUnknown;
}

FakeUdpSocket::FakeUdpSocket()
    : read_pending_(false),
      input_pos_(0),
      message_loop_(MessageLoop::current()) {
}

FakeUdpSocket::~FakeUdpSocket() {
  EXPECT_EQ(message_loop_, MessageLoop::current());
}

void FakeUdpSocket::AppendInputPacket(const char* data, int data_size) {
  EXPECT_EQ(message_loop_, MessageLoop::current());
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
  EXPECT_EQ(message_loop_, MessageLoop::current());
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
  EXPECT_EQ(message_loop_, MessageLoop::current());
  written_packets_.push_back(std::string());
  written_packets_.back().assign(buf->data(), buf->data() + buf_len);
  return buf_len;
}

bool FakeUdpSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}
bool FakeUdpSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return false;
}

FakeSession::FakeSession()
    : candidate_config_(CandidateSessionConfig::CreateDefault()),
      config_(SessionConfig::GetDefault()),
      message_loop_(NULL),
      jid_(kTestJid),
      error_(OK),
      closed_(false) {
}

FakeSession::~FakeSession() { }

FakeSocket* FakeSession::GetStreamChannel(const std::string& name) {
  return stream_channels_[name];
}

FakeUdpSocket* FakeSession::GetDatagramChannel(const std::string& name) {
  return datagram_channels_[name];
}

void FakeSession::SetStateChangeCallback(const StateChangeCallback& callback) {
  state_change_callback_ = callback;
}

void FakeSession::SetRouteChangeCallback(const RouteChangeCallback& callback) {
  route_change_callback_ = callback;
}

ErrorCode FakeSession::error() {
  return error_;
}

void FakeSession::CreateStreamChannel(
    const std::string& name, const StreamChannelCallback& callback) {
  scoped_ptr<FakeSocket> channel(new FakeSocket());
  stream_channels_[name] = channel.get();
  callback.Run(channel.PassAs<net::StreamSocket>());
}

void FakeSession::CreateDatagramChannel(
    const std::string& name, const DatagramChannelCallback& callback) {
  scoped_ptr<FakeUdpSocket> channel(new FakeUdpSocket());
  datagram_channels_[name] = channel.get();
  callback.Run(channel.PassAs<net::Socket>());
}

void FakeSession::CancelChannelCreation(const std::string& name) {
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

void FakeSession::Close() {
  closed_ = true;
}

}  // namespace protocol
}  // namespace remoting
