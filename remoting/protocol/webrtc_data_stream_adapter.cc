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
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

class WebrtcDataStreamAdapter::Channel : public MessagePipe,
                                         public webrtc::DataChannelObserver {
 public:
  explicit Channel(base::WeakPtr<WebrtcDataStreamAdapter> adapter);
  ~Channel() override;

  void Start(rtc::scoped_refptr<webrtc::DataChannelInterface> channel);

  std::string name() { return channel_->label(); }

  // MessagePipe interface.
  void StartReceiving(const MessageReceivedCallback& callback) override;
  void Send(google::protobuf::MessageLite* message,
            const base::Closure& done) override;

 private:
  enum class State { CONNECTING, OPEN, CLOSED };

  // webrtc::DataChannelObserver interface.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

  void OnConnected();

  void OnError();

  base::WeakPtr<WebrtcDataStreamAdapter> adapter_;

  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;

  MessageReceivedCallback message_received_callback_;

  State state_ = State::CONNECTING;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

WebrtcDataStreamAdapter::Channel::Channel(
    base::WeakPtr<WebrtcDataStreamAdapter> adapter)
    : adapter_(adapter) {}

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
    OnConnected();
  } else {
    DCHECK_EQ(channel_->state(), webrtc::DataChannelInterface::kConnecting);
  }
}

void WebrtcDataStreamAdapter::Channel::StartReceiving(
    const MessageReceivedCallback& callback) {
  DCHECK(message_received_callback_.is_null());
  DCHECK(!callback.is_null());

  message_received_callback_ = callback;
}

void WebrtcDataStreamAdapter::Channel::Send(
    google::protobuf::MessageLite* message,
    const base::Closure& done) {
  rtc::Buffer buffer;
  buffer.SetSize(message->ByteSize());
  message->SerializeWithCachedSizesToArray(
      reinterpret_cast<uint8_t*>(buffer.data()));
  webrtc::DataBuffer data_buffer(std::move(buffer), true /* binary */);
  if (!channel_->Send(data_buffer)) {
    OnError();
    return;
  }

  if (!done.is_null())
    done.Run();
}

void WebrtcDataStreamAdapter::Channel::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kOpen:
      OnConnected();
      break;

    case webrtc::DataChannelInterface::kClosing:
      // Currently channels are not expected to be closed.
      OnError();
      break;

    case webrtc::DataChannelInterface::kConnecting:
    case webrtc::DataChannelInterface::kClosed:
      break;
  }
}

void WebrtcDataStreamAdapter::Channel::OnConnected() {
  CHECK(state_ == State::CONNECTING);
  state_ = State::OPEN;
  adapter_->OnChannelConnected(this);
}

void WebrtcDataStreamAdapter::Channel::OnError() {
  if (state_ == State::CLOSED)
    return;

  state_ = State::CLOSED;
  if (adapter_)
    adapter_->OnChannelError(this);
}

void WebrtcDataStreamAdapter::Channel::OnMessage(
    const webrtc::DataBuffer& rtc_buffer) {
  scoped_ptr<CompoundBuffer> buffer(new CompoundBuffer());
  buffer->AppendCopyOf(reinterpret_cast<const char*>(rtc_buffer.data.data()),
                       rtc_buffer.data.size());
  buffer->Lock();
  message_received_callback_.Run(std::move(buffer));
}

struct WebrtcDataStreamAdapter::PendingChannel {
  PendingChannel() {}
  PendingChannel(scoped_ptr<Channel> channel,
                 const ChannelCreatedCallback& connected_callback)
      : channel(std::move(channel)), connected_callback(connected_callback) {}
  PendingChannel(PendingChannel&& other)
      : channel(std::move(other.channel)),
        connected_callback(std::move(other.connected_callback)) {}
  PendingChannel& operator=(PendingChannel&& other) {
    channel = std::move(other.channel);
    connected_callback = std::move(other.connected_callback);
    return *this;
  }

  scoped_ptr<Channel> channel;
  ChannelCreatedCallback connected_callback;
};

WebrtcDataStreamAdapter::WebrtcDataStreamAdapter(
    bool outgoing,
    const ErrorCallback& error_callback)
    : outgoing_(outgoing),
      error_callback_(error_callback),
      weak_factory_(this) {}

WebrtcDataStreamAdapter::~WebrtcDataStreamAdapter() {
  DCHECK(pending_channels_.empty());
}

void WebrtcDataStreamAdapter::Initialize(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
  peer_connection_ = peer_connection;
}

void WebrtcDataStreamAdapter::OnIncomingDataChannel(
    webrtc::DataChannelInterface* data_channel) {
  DCHECK(!outgoing_);

  auto it = pending_channels_.find(data_channel->label());
  if (it == pending_channels_.end()) {
    LOG(ERROR) << "Received unexpected data channel " << data_channel->label();
    return;
  }
  it->second.channel->Start(data_channel);
}

void WebrtcDataStreamAdapter::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  DCHECK(peer_connection_);
  DCHECK(pending_channels_.find(name) == pending_channels_.end());

  Channel* channel = new Channel(weak_factory_.GetWeakPtr());
  pending_channels_[name] = PendingChannel(make_scoped_ptr(channel), callback);

  if (outgoing_) {
    webrtc::DataChannelInit config;
    config.reliable = true;
    channel->Start(peer_connection_->CreateDataChannel(name, &config));
  }
}

void WebrtcDataStreamAdapter::CancelChannelCreation(const std::string& name) {
  auto it = pending_channels_.find(name);
  DCHECK(it != pending_channels_.end());
  pending_channels_.erase(it);
}

void WebrtcDataStreamAdapter::OnChannelConnected(Channel* channel) {
  auto it = pending_channels_.find(channel->name());
  DCHECK(it != pending_channels_.end());
  PendingChannel pending_channel = std::move(it->second);
  pending_channels_.erase(it);

  // Once the channel is connected its ownership  is passed to the
  // |connected_callback|.
  pending_channel.connected_callback.Run(std::move(pending_channel.channel));
}

void WebrtcDataStreamAdapter::OnChannelError(Channel* channel) {
  error_callback_.Run(CHANNEL_CONNECTION_ERROR);
}

}  // namespace protocol
}  // namespace remoting
