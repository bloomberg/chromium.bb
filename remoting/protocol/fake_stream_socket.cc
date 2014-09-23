// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_stream_socket.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

FakeStreamSocket::FakeStreamSocket()
    : async_write_(false),
      write_pending_(false),
      write_limit_(0),
      next_write_error_(net::OK),
      next_read_error_(net::OK),
      read_buffer_size_(0),
      input_pos_(0),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
}

FakeStreamSocket::~FakeStreamSocket() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
}

void FakeStreamSocket::AppendInputData(const std::string& data) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  input_data_.insert(input_data_.end(), data.begin(), data.end());
  // Complete pending read if any.
  if (!read_callback_.is_null()) {
    int result = std::min(read_buffer_size_,
                          static_cast<int>(input_data_.size() - input_pos_));
    EXPECT_GT(result, 0);
    memcpy(read_buffer_->data(),
           &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    read_buffer_ = NULL;

    net::CompletionCallback callback = read_callback_;
    read_callback_.Reset();
    callback.Run(result);
  }
}

void FakeStreamSocket::PairWith(FakeStreamSocket* peer_socket) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  peer_socket_ = peer_socket->GetWeakPtr();
  peer_socket->peer_socket_ = GetWeakPtr();
}

base::WeakPtr<FakeStreamSocket> FakeStreamSocket::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int FakeStreamSocket::Read(net::IOBuffer* buf, int buf_len,
                           const net::CompletionCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

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
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeStreamSocket::Write(net::IOBuffer* buf, int buf_len,
                      const net::CompletionCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  EXPECT_FALSE(write_pending_);

  if (write_limit_ > 0)
    buf_len = std::min(write_limit_, buf_len);

  if (async_write_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FakeStreamSocket::DoAsyncWrite, weak_factory_.GetWeakPtr(),
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

void FakeStreamSocket::DoAsyncWrite(scoped_refptr<net::IOBuffer> buf,
                                    int buf_len,
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

void FakeStreamSocket::DoWrite(net::IOBuffer* buf, int buf_len) {
  written_data_.insert(written_data_.end(),
                       buf->data(), buf->data() + buf_len);

  if (peer_socket_.get()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FakeStreamSocket::AppendInputData,
                   peer_socket_,
                   std::string(buf->data(), buf->data() + buf_len)));
  }
}

int FakeStreamSocket::SetReceiveBufferSize(int32 size) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeStreamSocket::SetSendBufferSize(int32 size) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeStreamSocket::Connect(const net::CompletionCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  return net::OK;
}

void FakeStreamSocket::Disconnect() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  peer_socket_.reset();
}

bool FakeStreamSocket::IsConnected() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  return true;
}

bool FakeStreamSocket::IsConnectedAndIdle() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return false;
}

int FakeStreamSocket::GetPeerAddress(net::IPEndPoint* address) const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  net::IPAddressNumber ip(net::kIPv4AddressSize);
  *address = net::IPEndPoint(ip, 0);
  return net::OK;
}

int FakeStreamSocket::GetLocalAddress(net::IPEndPoint* address) const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

const net::BoundNetLog& FakeStreamSocket::NetLog() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  return net_log_;
}

void FakeStreamSocket::SetSubresourceSpeculation() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void FakeStreamSocket::SetOmniboxSpeculation() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

bool FakeStreamSocket::WasEverUsed() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return true;
}

bool FakeStreamSocket::UsingTCPFastOpen() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return true;
}

bool FakeStreamSocket::WasNpnNegotiated() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  return false;
}

net::NextProto FakeStreamSocket::GetNegotiatedProtocol() const {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
  return net::kProtoUnknown;
}

bool FakeStreamSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  return false;
}

FakeStreamChannelFactory::FakeStreamChannelFactory()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      asynchronous_create_(false),
      fail_create_(false),
      weak_factory_(this) {
}

FakeStreamChannelFactory::~FakeStreamChannelFactory() {}

FakeStreamSocket* FakeStreamChannelFactory::GetFakeChannel(
    const std::string& name) {
  return channels_[name].get();
}

void FakeStreamChannelFactory::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  scoped_ptr<FakeStreamSocket> channel(new FakeStreamSocket());
  channels_[name] = channel->GetWeakPtr();

  if (fail_create_)
    channel.reset();

  if (asynchronous_create_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FakeStreamChannelFactory::NotifyChannelCreated,
        weak_factory_.GetWeakPtr(), base::Passed(&channel), name, callback));
  } else {
    NotifyChannelCreated(channel.Pass(), name, callback);
  }
}

void FakeStreamChannelFactory::NotifyChannelCreated(
    scoped_ptr<FakeStreamSocket> owned_channel,
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  if (channels_.find(name) != channels_.end())
    callback.Run(owned_channel.PassAs<net::StreamSocket>());
}

void FakeStreamChannelFactory::CancelChannelCreation(const std::string& name) {
  channels_.erase(name);
}

}  // namespace protocol
}  // namespace remoting
