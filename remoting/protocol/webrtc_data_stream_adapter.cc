// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_data_stream_adapter.h"

#include <stdint.h>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/p2p_stream_socket.h"

static const int kMaxSendBufferSize = 256 * 1024;

namespace remoting {
namespace protocol {

class WebrtcDataStreamAdapter::Channel : public P2PStreamSocket,
                                         public webrtc::DataChannelObserver {
 public:
  typedef base::Callback<void(Channel* adapter, bool success)>
      ConnectedCallback;

  Channel(const ConnectedCallback& connected_callback);
  ~Channel() override;

  void Start(rtc::scoped_refptr<webrtc::DataChannelInterface> channel);

  std::string name() { return channel_->label(); }

  // P2PStreamSocket interface.
  int Read(const scoped_refptr<net::IOBuffer>& buffer, int buffer_size,
           const net::CompletionCallback &callback) override;
  int Write(const scoped_refptr<net::IOBuffer>& buffer, int buffer_size,
            const net::CompletionCallback &callback) override;

 private:
  // webrtc::DataChannelObserver interface.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;
  void OnBufferedAmountChange(uint64_t previous_amount) override;

  int DoWrite(const scoped_refptr<net::IOBuffer>& buffer, int buffer_size);

  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;
  ConnectedCallback connected_callback_;

  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;
  net::CompletionCallback write_callback_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback read_callback_;

  CompoundBuffer received_data_buffer_;

  base::WeakPtrFactory<Channel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

WebrtcDataStreamAdapter::Channel::Channel(
    const ConnectedCallback& connected_callback)
    : connected_callback_(connected_callback), weak_factory_(this) {}

WebrtcDataStreamAdapter::Channel::~Channel() {
  if (channel_) {
    channel_->UnregisterObserver();
    channel_->Close();
  }
}

void WebrtcDataStreamAdapter::Channel::Start(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  DCHECK(!channel_);

  channel_ = channel;
  channel_->RegisterObserver(this);

   if (channel_->state() == webrtc::DataChannelInterface::kOpen) {
    base::ResetAndReturn(&connected_callback_).Run(this, true);
  } else {
    DCHECK_EQ(channel_->state(), webrtc::DataChannelInterface::kConnecting);
  }
}

int WebrtcDataStreamAdapter::Channel::Read(
    const scoped_refptr<net::IOBuffer>& buffer, int buffer_size,
    const net::CompletionCallback& callback) {
  DCHECK(read_callback_.is_null());

  if (received_data_buffer_.total_bytes() == 0) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_size;
    read_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  int bytes_to_copy =
      std::min(buffer_size, received_data_buffer_.total_bytes());
  received_data_buffer_.CopyTo(buffer->data(), bytes_to_copy);
  received_data_buffer_.CropFront(bytes_to_copy);
  return bytes_to_copy;
}

int WebrtcDataStreamAdapter::Channel::Write(
    const scoped_refptr<net::IOBuffer>& buffer, int buffer_size,
    const net::CompletionCallback& callback) {
  DCHECK(write_callback_.is_null());

  if (channel_->buffered_amount() >= kMaxSendBufferSize) {
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
    write_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  return DoWrite(buffer, buffer_size);
}

void WebrtcDataStreamAdapter::Channel::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kConnecting:
      break;

    case webrtc::DataChannelInterface::kOpen:
      DCHECK(!connected_callback_.is_null());
      base::ResetAndReturn(&connected_callback_).Run(this, true);
      break;

    case webrtc::DataChannelInterface::kClosing: {
      // Hold weak pointer for self to detect when one of the callbacks deletes
      // the channel.
      base::WeakPtr<Channel> self = weak_factory_.GetWeakPtr();
      if (!connected_callback_.is_null()) {
        base::ResetAndReturn(&connected_callback_).Run(this, false);
      }

      if (self && !read_callback_.is_null()) {
        read_buffer_ = nullptr;
        base::ResetAndReturn(&read_callback_).Run(net::ERR_CONNECTION_CLOSED);
      }

      if (self && !write_callback_.is_null()) {
        write_buffer_ = nullptr;
        base::ResetAndReturn(&write_callback_).Run(net::ERR_CONNECTION_CLOSED);
      }
      break;
    }
    case webrtc::DataChannelInterface::kClosed:
      DCHECK(connected_callback_.is_null());
      break;
  }
}

void WebrtcDataStreamAdapter::Channel::OnMessage(
    const webrtc::DataBuffer& buffer) {
  const char* data = reinterpret_cast<const char*>(buffer.data.data());
  int data_size = buffer.data.size();

  // If there is no outstanding read request then just copy the data to
  // |received_data_buffer_|.
  if (read_callback_.is_null()) {
    received_data_buffer_.AppendCopyOf(data, data_size);
    return;
  }

  DCHECK(received_data_buffer_.total_bytes() == 0);
  int bytes_to_copy = std::min(read_buffer_size_, data_size);
  memcpy(read_buffer_->data(), buffer.data.data(), bytes_to_copy);

  if (bytes_to_copy < data_size) {
    received_data_buffer_.AppendCopyOf(data + bytes_to_copy,
                                       data_size - bytes_to_copy);
  }
  read_buffer_ = nullptr;
  base::ResetAndReturn(&read_callback_).Run(bytes_to_copy);
}

void WebrtcDataStreamAdapter::Channel::OnBufferedAmountChange(
    uint64_t previous_amount) {
  if (channel_->buffered_amount() < kMaxSendBufferSize) {
    base::ResetAndReturn(&write_callback_)
        .Run(DoWrite(write_buffer_, write_buffer_size_));
  }
}

int WebrtcDataStreamAdapter::Channel::DoWrite(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_size) {
  webrtc::DataBuffer data_buffer(rtc::Buffer(buffer->data(), buffer_size),
                                 true /* binary */);
  if (channel_->Send(data_buffer)) {
    return buffer_size;
  } else {
    return net::ERR_FAILED;
  }
}

WebrtcDataStreamAdapter::WebrtcDataStreamAdapter() : weak_factory_(this) {}

WebrtcDataStreamAdapter::~WebrtcDataStreamAdapter() {
  DCHECK(pending_channels_.empty());
}

void WebrtcDataStreamAdapter::Initialize(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection,
    bool outgoing) {
  peer_connection_ = peer_connection;
  outgoing_ = outgoing;

 if (outgoing_) {
    for (auto& channel : pending_channels_) {
      webrtc::DataChannelInit config;
      config.reliable = true;
      channel.second->Start(
          peer_connection_->CreateDataChannel(channel.first, &config));
    }
 }
}

void WebrtcDataStreamAdapter::OnIncomingDataChannel(
    webrtc::DataChannelInterface* data_channel) {
  auto it = pending_channels_.find(data_channel->label());
  if (outgoing_ || it == pending_channels_.end()) {
    LOG(ERROR) << "Received unexpected data channel " << data_channel->label();
    return;
  }
  it->second->Start(data_channel);
}

void WebrtcDataStreamAdapter::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  DCHECK(pending_channels_.find(name) == pending_channels_.end());

  Channel* channel =
      new Channel(base::Bind(&WebrtcDataStreamAdapter::OnChannelConnected,
                             base::Unretained(this), callback));
  pending_channels_[name] = channel;

  if (peer_connection_ && outgoing_) {
    webrtc::DataChannelInit config;
    config.reliable = true;
    channel->Start(peer_connection_->CreateDataChannel(name, &config));
  }
}

void WebrtcDataStreamAdapter::CancelChannelCreation(const std::string& name) {
  auto it = pending_channels_.find(name);
  DCHECK(it != pending_channels_.end());
  delete it->second;
  pending_channels_.erase(it);
}

void WebrtcDataStreamAdapter::OnChannelConnected(
    const ChannelCreatedCallback& connected_callback,
    Channel* channel,
    bool connected) {
  auto it = pending_channels_.find(channel->name());
  DCHECK(it != pending_channels_.end());
  pending_channels_.erase(it);

  // The callback can delete the channel which also holds the callback
  // object which may cause crash if the callback carries some arguments. Copy
  // the callback to stack to avoid this problem.
  ChannelCreatedCallback callback = connected_callback;
  callback.Run(make_scoped_ptr(channel));
}

}  // namespace protocol
}  // namespace remoting
