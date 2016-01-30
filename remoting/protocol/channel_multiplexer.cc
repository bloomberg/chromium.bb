// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_multiplexer.h"

#include <stddef.h>
#include <string.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting {
namespace protocol {

namespace {
const int kChannelIdUnknown = -1;
const int kMaxPacketSize = 1024;

class PendingPacket {
 public:
  PendingPacket(scoped_ptr<MultiplexPacket> packet)
      : packet(std::move(packet)) {}
  ~PendingPacket() {}

  bool is_empty() { return pos >= packet->data().size(); }

  int Read(char* buffer, size_t size) {
    size = std::min(size, packet->data().size() - pos);
    memcpy(buffer, packet->data().data() + pos, size);
    pos += size;
    return size;
  }

 private:
  scoped_ptr<MultiplexPacket> packet;
  size_t pos = 0U;

  DISALLOW_COPY_AND_ASSIGN(PendingPacket);
};

}  // namespace

const char ChannelMultiplexer::kMuxChannelName[] = "mux";

struct ChannelMultiplexer::PendingChannel {
  PendingChannel(const std::string& name,
                 const ChannelCreatedCallback& callback)
      : name(name), callback(callback) {
  }
  std::string name;
  ChannelCreatedCallback callback;
};

class ChannelMultiplexer::MuxChannel {
 public:
  MuxChannel(ChannelMultiplexer* multiplexer, const std::string& name,
             int send_id);
  ~MuxChannel();

  const std::string& name() { return name_; }
  int receive_id() { return receive_id_; }
  void set_receive_id(int id) { receive_id_ = id; }

  // Called by ChannelMultiplexer.
  scoped_ptr<P2PStreamSocket> CreateSocket();
  void OnIncomingPacket(scoped_ptr<MultiplexPacket> packet);
  void OnBaseChannelError(int error);

  // Called by MuxSocket.
  void OnSocketDestroyed();
  void DoWrite(scoped_ptr<MultiplexPacket> packet,
               const base::Closure& done_task);
  int DoRead(const scoped_refptr<net::IOBuffer>& buffer, int buffer_len);

 private:
  ChannelMultiplexer* multiplexer_;
  std::string name_;
  int send_id_;
  bool id_sent_;
  int receive_id_;
  MuxSocket* socket_;
  std::list<PendingPacket*> pending_packets_;

  DISALLOW_COPY_AND_ASSIGN(MuxChannel);
};

class ChannelMultiplexer::MuxSocket : public P2PStreamSocket,
                                      public base::NonThreadSafe,
                                      public base::SupportsWeakPtr<MuxSocket> {
 public:
  MuxSocket(MuxChannel* channel);
  ~MuxSocket() override;

  void OnWriteComplete();
  void OnBaseChannelError(int error);
  void OnPacketReceived();

  // P2PStreamSocket interface.
  int Read(const scoped_refptr<net::IOBuffer>& buffer, int buffer_len,
           const net::CompletionCallback& callback) override;
  int Write(const scoped_refptr<net::IOBuffer>& buffer, int buffer_len,
            const net::CompletionCallback& callback) override;

 private:
  MuxChannel* channel_;

  int base_channel_error_ = net::OK;

  net::CompletionCallback read_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  bool write_pending_;
  int write_result_;
  net::CompletionCallback write_callback_;

  DISALLOW_COPY_AND_ASSIGN(MuxSocket);
};


ChannelMultiplexer::MuxChannel::MuxChannel(
    ChannelMultiplexer* multiplexer,
    const std::string& name,
    int send_id)
    : multiplexer_(multiplexer),
      name_(name),
      send_id_(send_id),
      id_sent_(false),
      receive_id_(kChannelIdUnknown),
      socket_(nullptr) {
}

ChannelMultiplexer::MuxChannel::~MuxChannel() {
  // Socket must be destroyed before the channel.
  DCHECK(!socket_);
  STLDeleteElements(&pending_packets_);
}

scoped_ptr<P2PStreamSocket> ChannelMultiplexer::MuxChannel::CreateSocket() {
  DCHECK(!socket_);  // Can't create more than one socket per channel.
  scoped_ptr<MuxSocket> result(new MuxSocket(this));
  socket_ = result.get();
  return std::move(result);
}

void ChannelMultiplexer::MuxChannel::OnIncomingPacket(
    scoped_ptr<MultiplexPacket> packet) {
  DCHECK_EQ(packet->channel_id(), receive_id_);
  if (packet->data().size() > 0) {
    pending_packets_.push_back(new PendingPacket(std::move(packet)));
    if (socket_) {
      // Notify the socket that we have more data.
      socket_->OnPacketReceived();
    }
  }
}

void ChannelMultiplexer::MuxChannel::OnBaseChannelError(int error) {
  if (socket_)
    socket_->OnBaseChannelError(error);
}

void ChannelMultiplexer::MuxChannel::OnSocketDestroyed() {
  DCHECK(socket_);
  socket_ = nullptr;
}

void ChannelMultiplexer::MuxChannel::DoWrite(
    scoped_ptr<MultiplexPacket> packet,
    const base::Closure& done_task) {
  packet->set_channel_id(send_id_);
  if (!id_sent_) {
    packet->set_channel_name(name_);
    id_sent_ = true;
  }
  multiplexer_->DoWrite(std::move(packet), done_task);
}

int ChannelMultiplexer::MuxChannel::DoRead(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_len) {
  int pos = 0;
  while (buffer_len > 0 && !pending_packets_.empty()) {
    DCHECK(!pending_packets_.front()->is_empty());
    int result = pending_packets_.front()->Read(
        buffer->data() + pos, buffer_len);
    DCHECK_LE(result, buffer_len);
    pos += result;
    buffer_len -= pos;
    if (pending_packets_.front()->is_empty()) {
      delete pending_packets_.front();
      pending_packets_.erase(pending_packets_.begin());
    }
  }
  return pos;
}

ChannelMultiplexer::MuxSocket::MuxSocket(MuxChannel* channel)
    : channel_(channel),
      read_buffer_size_(0),
      write_pending_(false),
      write_result_(0) {
}

ChannelMultiplexer::MuxSocket::~MuxSocket() {
  channel_->OnSocketDestroyed();
}

int ChannelMultiplexer::MuxSocket::Read(
    const scoped_refptr<net::IOBuffer>& buffer, int buffer_len,
    const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(read_callback_.is_null());

  if (base_channel_error_ != net::OK)
    return base_channel_error_;

  int result = channel_->DoRead(buffer, buffer_len);
  if (result == 0) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_len;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }
  return result;
}

int ChannelMultiplexer::MuxSocket::Write(
    const scoped_refptr<net::IOBuffer>& buffer, int buffer_len,
    const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(write_callback_.is_null());

  if (base_channel_error_ != net::OK)
    return base_channel_error_;

  scoped_ptr<MultiplexPacket> packet(new MultiplexPacket());
  size_t size = std::min(kMaxPacketSize, buffer_len);
  packet->mutable_data()->assign(buffer->data(), size);

  write_pending_ = true;
  channel_->DoWrite(std::move(packet), base::Bind(
      &ChannelMultiplexer::MuxSocket::OnWriteComplete, AsWeakPtr()));

  // OnWriteComplete() might be called above synchronously.
  if (write_pending_) {
    DCHECK(write_callback_.is_null());
    write_callback_ = callback;
    write_result_ = size;
    return net::ERR_IO_PENDING;
  }

  return size;
}

void ChannelMultiplexer::MuxSocket::OnWriteComplete() {
  write_pending_ = false;
  if (!write_callback_.is_null())
    base::ResetAndReturn(&write_callback_).Run(write_result_);

}

void ChannelMultiplexer::MuxSocket::OnBaseChannelError(int error) {
  base_channel_error_ = error;

  // Here only one of the read and write callbacks is called if both of them are
  // pending. Ideally both of them should be called in that case, but that would
  // require the second one to be called asynchronously which would complicate
  // this code. Channels handle read and write errors the same way (see
  // ChannelDispatcherBase::OnReadWriteFailed) so calling only one of the
  // callbacks is enough.

  if (!read_callback_.is_null()) {
    base::ResetAndReturn(&read_callback_).Run(error);
    return;
  }

  if (!write_callback_.is_null())
    base::ResetAndReturn(&write_callback_).Run(error);
}

void ChannelMultiplexer::MuxSocket::OnPacketReceived() {
  if (!read_callback_.is_null()) {
    int result = channel_->DoRead(read_buffer_.get(), read_buffer_size_);
    read_buffer_ = nullptr;
    DCHECK_GT(result, 0);
    base::ResetAndReturn(&read_callback_).Run(result);
  }
}

ChannelMultiplexer::ChannelMultiplexer(StreamChannelFactory* factory,
                                       const std::string& base_channel_name)
    : base_channel_factory_(factory),
      base_channel_name_(base_channel_name),
      next_channel_id_(0),
      weak_factory_(this) {}

ChannelMultiplexer::~ChannelMultiplexer() {
  DCHECK(pending_channels_.empty());
  STLDeleteValues(&channels_);

  // Cancel creation of the base channel if it hasn't finished.
  if (base_channel_factory_)
    base_channel_factory_->CancelChannelCreation(base_channel_name_);
}

void ChannelMultiplexer::CreateChannel(const std::string& name,
                                       const ChannelCreatedCallback& callback) {
  if (base_channel_.get()) {
    // Already have |base_channel_|. Create new multiplexed channel
    // synchronously.
    callback.Run(GetOrCreateChannel(name)->CreateSocket());
  } else if (!base_channel_.get() && !base_channel_factory_) {
    // Fail synchronously if we failed to create |base_channel_|.
    callback.Run(nullptr);
  } else {
    // Still waiting for the |base_channel_|.
    pending_channels_.push_back(PendingChannel(name, callback));

    // If this is the first multiplexed channel then create the base channel.
    if (pending_channels_.size() == 1U) {
      base_channel_factory_->CreateChannel(
          base_channel_name_,
          base::Bind(&ChannelMultiplexer::OnBaseChannelReady,
                     base::Unretained(this)));
    }
  }
}

void ChannelMultiplexer::CancelChannelCreation(const std::string& name) {
  for (std::list<PendingChannel>::iterator it = pending_channels_.begin();
       it != pending_channels_.end(); ++it) {
    if (it->name == name) {
      pending_channels_.erase(it);
      return;
    }
  }
}

void ChannelMultiplexer::OnBaseChannelReady(
    scoped_ptr<P2PStreamSocket> socket) {
  base_channel_factory_ = nullptr;
  base_channel_ = std::move(socket);

  if (base_channel_.get()) {
    // Initialize reader and writer.
    reader_.StartReading(base_channel_.get(),
                         base::Bind(&ChannelMultiplexer::OnIncomingPacket,
                                    base::Unretained(this)),
                         base::Bind(&ChannelMultiplexer::OnBaseChannelError,
                                    base::Unretained(this)));
    writer_.Start(base::Bind(&P2PStreamSocket::Write,
                             base::Unretained(base_channel_.get())),
                  base::Bind(&ChannelMultiplexer::OnBaseChannelError,
                             base::Unretained(this)));
  }

  DoCreatePendingChannels();
}

void ChannelMultiplexer::DoCreatePendingChannels() {
  if (pending_channels_.empty())
    return;

  // Every time this function is called it connects a single channel and posts a
  // separate task to connect other channels. This is necessary because the
  // callback may destroy the multiplexer or somehow else modify
  // |pending_channels_| list (e.g. call CancelChannelCreation()).
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ChannelMultiplexer::DoCreatePendingChannels,
                            weak_factory_.GetWeakPtr()));

  PendingChannel c = pending_channels_.front();
  pending_channels_.erase(pending_channels_.begin());
  scoped_ptr<P2PStreamSocket> socket;
  if (base_channel_.get())
    socket = GetOrCreateChannel(c.name)->CreateSocket();
  c.callback.Run(std::move(socket));
}

ChannelMultiplexer::MuxChannel* ChannelMultiplexer::GetOrCreateChannel(
    const std::string& name) {
  // Check if we already have a channel with the requested name.
  std::map<std::string, MuxChannel*>::iterator it = channels_.find(name);
  if (it != channels_.end())
    return it->second;

  // Create a new channel if we haven't found existing one.
  MuxChannel* channel = new MuxChannel(this, name, next_channel_id_);
  ++next_channel_id_;
  channels_[channel->name()] = channel;
  return channel;
}


void ChannelMultiplexer::OnBaseChannelError(int error) {
  for (std::map<std::string, MuxChannel*>::iterator it = channels_.begin();
       it != channels_.end(); ++it) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ChannelMultiplexer::NotifyBaseChannelError,
                   weak_factory_.GetWeakPtr(), it->second->name(), error));
  }
}

void ChannelMultiplexer::NotifyBaseChannelError(const std::string& name,
                                                int error) {
  std::map<std::string, MuxChannel*>::iterator it = channels_.find(name);
  if (it != channels_.end())
    it->second->OnBaseChannelError(error);
}

void ChannelMultiplexer::OnIncomingPacket(scoped_ptr<CompoundBuffer> buffer) {
  scoped_ptr<MultiplexPacket> packet =
      ParseMessage<MultiplexPacket>(buffer.get());
  if (!packet)
    return;

  DCHECK(packet->has_channel_id());
  if (!packet->has_channel_id()) {
    LOG(ERROR) << "Received packet without channel_id.";
    return;
  }

  int receive_id = packet->channel_id();
  MuxChannel* channel = nullptr;
  std::map<int, MuxChannel*>::iterator it =
      channels_by_receive_id_.find(receive_id);
  if (it != channels_by_receive_id_.end()) {
    channel = it->second;
  } else {
    // This is a new |channel_id| we haven't seen before. Look it up by name.
    if (!packet->has_channel_name()) {
      LOG(ERROR) << "Received packet with unknown channel_id and "
          "without channel_name.";
      return;
    }
    channel = GetOrCreateChannel(packet->channel_name());
    channel->set_receive_id(receive_id);
    channels_by_receive_id_[receive_id] = channel;
  }

  channel->OnIncomingPacket(std::move(packet));
}

void ChannelMultiplexer::DoWrite(scoped_ptr<MultiplexPacket> packet,
                                 const base::Closure& done_task) {
  writer_.Write(SerializeAndFrameMessage(*packet), done_task);
}

}  // namespace protocol
}  // namespace remoting
