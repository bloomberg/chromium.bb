// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_datagram_socket.h"

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

FakeDatagramSocket::FakeDatagramSocket()
    : input_pos_(0),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
}

FakeDatagramSocket::~FakeDatagramSocket() {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
}

void FakeDatagramSocket::AppendInputPacket(const std::string& data) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  input_packets_.push_back(data);

  // Complete pending read if any.
  if (!read_callback_.is_null()) {
    DCHECK_EQ(input_pos_, static_cast<int>(input_packets_.size()) - 1);
    int result = CopyReadData(read_buffer_.get(), read_buffer_size_);
    read_buffer_ = NULL;

    net::CompletionCallback callback = read_callback_;
    read_callback_.Reset();
    callback.Run(result);
  }
}

void FakeDatagramSocket::PairWith(FakeDatagramSocket* peer_socket) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  peer_socket_ = peer_socket->GetWeakPtr();
  peer_socket->peer_socket_ = GetWeakPtr();
}

base::WeakPtr<FakeDatagramSocket> FakeDatagramSocket::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int FakeDatagramSocket::Read(net::IOBuffer* buf, int buf_len,
                             const net::CompletionCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  if (input_pos_ < static_cast<int>(input_packets_.size())) {
    return CopyReadData(buf, buf_len);
  } else {
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
}

int FakeDatagramSocket::Write(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback) {
  EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  written_packets_.push_back(std::string());
  written_packets_.back().assign(buf->data(), buf->data() + buf_len);

  if (peer_socket_.get()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FakeDatagramSocket::AppendInputPacket,
                   peer_socket_,
                   std::string(buf->data(), buf->data() + buf_len)));
  }

  return buf_len;
}

int FakeDatagramSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeDatagramSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeDatagramSocket::CopyReadData(net::IOBuffer* buf, int buf_len) {
  int size = std::min(
      buf_len, static_cast<int>(input_packets_[input_pos_].size()));
  memcpy(buf->data(), &(*input_packets_[input_pos_].begin()), size);
  ++input_pos_;
  return size;
}

FakeDatagramChannelFactory::FakeDatagramChannelFactory()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      asynchronous_create_(false),
      fail_create_(false),
      weak_factory_(this) {
}

FakeDatagramChannelFactory::~FakeDatagramChannelFactory() {
  for (ChannelsMap::iterator it = channels_.begin(); it != channels_.end();
       ++it) {
    EXPECT_TRUE(it->second == NULL);
  }
}

void FakeDatagramChannelFactory::PairWith(
    FakeDatagramChannelFactory* peer_factory) {
  peer_factory_ = peer_factory->weak_factory_.GetWeakPtr();
  peer_factory_->peer_factory_ = weak_factory_.GetWeakPtr();
}

FakeDatagramSocket* FakeDatagramChannelFactory::GetFakeChannel(
    const std::string& name) {
  return channels_[name].get();
}

void FakeDatagramChannelFactory::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  EXPECT_TRUE(channels_[name] == NULL);

  scoped_ptr<FakeDatagramSocket> channel(new FakeDatagramSocket());
  channels_[name] = channel->GetWeakPtr();

  if (peer_factory_) {
    FakeDatagramSocket* peer_socket = peer_factory_->GetFakeChannel(name);
    if (peer_socket)
      channel->PairWith(peer_socket);
  }

  if (fail_create_)
    channel.reset();

  if (asynchronous_create_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FakeDatagramChannelFactory::NotifyChannelCreated,
                   weak_factory_.GetWeakPtr(), base::Passed(&channel),
                   name, callback));
  } else {
    NotifyChannelCreated(channel.Pass(), name, callback);
  }
}

void FakeDatagramChannelFactory::NotifyChannelCreated(
    scoped_ptr<FakeDatagramSocket> owned_socket,
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  if (channels_.find(name) != channels_.end())
    callback.Run(owned_socket.PassAs<net::Socket>());
}

void FakeDatagramChannelFactory::CancelChannelCreation(
    const std::string& name) {
  channels_.erase(name);
}

}  // namespace protocol
}  // namespace remoting
