// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_data_stream_adapter.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

class WebrtcDataStreamAdapter::Channel : public MessagePipe,
                                         public webrtc::DataChannelObserver {
 public:
  explicit Channel(WebrtcDataStreamAdapter* adapter);
  ~Channel() override;

  void Start(rtc::scoped_refptr<webrtc::DataChannelInterface> channel);

  std::string name() { return channel_->label(); }

  // MessagePipe interface.
  void Start(EventHandler* event_handler) override;
  void Send(google::protobuf::MessageLite* message,
            const base::Closure& done) override;

 private:
  enum class State { CONNECTING, OPEN, CLOSED };

  // webrtc::DataChannelObserver interface.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

  void OnConnected();

  void OnClosed();

  // |adapter_| owns channels while they are being connected.
  WebrtcDataStreamAdapter* adapter_;

  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;

  EventHandler* event_handler_ = nullptr;

  State state_ = State::CONNECTING;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

WebrtcDataStreamAdapter::Channel::Channel(WebrtcDataStreamAdapter* adapter)
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

void WebrtcDataStreamAdapter::Channel::Start(EventHandler* event_handler) {
  DCHECK(!event_handler_);
  DCHECK(event_handler);

  event_handler_ = event_handler;
}

void WebrtcDataStreamAdapter::Channel::Send(
    google::protobuf::MessageLite* message,
    const base::Closure& done) {
  DCHECK(state_ == State::OPEN);

  rtc::CopyOnWriteBuffer buffer;
  buffer.SetSize(message->ByteSize());
  message->SerializeWithCachedSizesToArray(
      reinterpret_cast<uint8_t*>(buffer.data()));
  webrtc::DataBuffer data_buffer(std::move(buffer), true /* binary */);
  if (!channel_->Send(data_buffer)) {
    LOG(ERROR) << "Send failed on data channel " << channel_->label();
    channel_->Close();
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
      OnClosed();
      break;

    case webrtc::DataChannelInterface::kConnecting:
    case webrtc::DataChannelInterface::kClosed:
      break;
  }
}

void WebrtcDataStreamAdapter::Channel::OnMessage(
    const webrtc::DataBuffer& rtc_buffer) {
  if (state_ != State::OPEN) {
    LOG(ERROR) << "Dropping a message received when the channel is not open.";
    return;
  }

  std::unique_ptr<CompoundBuffer> buffer(new CompoundBuffer());
  buffer->AppendCopyOf(reinterpret_cast<const char*>(rtc_buffer.data.data()),
                       rtc_buffer.data.size());
  buffer->Lock();
  event_handler_->OnMessageReceived(std::move(buffer));
}

void WebrtcDataStreamAdapter::Channel::OnConnected() {
  DCHECK(state_ == State::CONNECTING);
  state_ = State::OPEN;
  WebrtcDataStreamAdapter* adapter = adapter_;
  adapter_ = nullptr;
  adapter->OnChannelConnected(this);
}

void WebrtcDataStreamAdapter::Channel::OnClosed() {
  switch (state_) {
    case State::CONNECTING:
      state_ = State::CLOSED;
      LOG(WARNING) << "Channel " << channel_->label()
                   << " was closed before it's connected.";
      adapter_->OnChannelError();
      return;

    case State::OPEN:
      state_ = State::CLOSED;
      event_handler_->OnMessagePipeClosed();
      return;

    case State::CLOSED:
      break;
  }
}

struct WebrtcDataStreamAdapter::PendingChannel {
  PendingChannel(std::unique_ptr<Channel> channel,
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

  std::unique_ptr<Channel> channel;
  ChannelCreatedCallback connected_callback;
};

WebrtcDataStreamAdapter::WebrtcDataStreamAdapter(
    const ErrorCallback& error_callback)
    : error_callback_(error_callback) {}

WebrtcDataStreamAdapter::~WebrtcDataStreamAdapter() {
  DCHECK(pending_channels_.empty());
}

void WebrtcDataStreamAdapter::Initialize(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
  peer_connection_ = peer_connection;
}

void WebrtcDataStreamAdapter::WrapIncomingDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
    const ChannelCreatedCallback& callback) {
  AddPendingChannel(data_channel, callback);
}

void WebrtcDataStreamAdapter::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  webrtc::DataChannelInit config;
  config.reliable = true;
  AddPendingChannel(peer_connection_->CreateDataChannel(name, &config),
                    callback);
}

void WebrtcDataStreamAdapter::CancelChannelCreation(const std::string& name) {
  auto it = pending_channels_.find(name);
  DCHECK(it != pending_channels_.end());
  pending_channels_.erase(it);
}

void WebrtcDataStreamAdapter::AddPendingChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
    const ChannelCreatedCallback& callback) {
  DCHECK(peer_connection_);
  DCHECK(pending_channels_.find(data_channel->label()) ==
         pending_channels_.end());

  Channel* channel = new Channel(this);
  pending_channels_.insert(
      std::make_pair(data_channel->label(),
                     PendingChannel(base::WrapUnique(channel), callback)));

  channel->Start(data_channel);
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

void WebrtcDataStreamAdapter::OnChannelError() {
  pending_channels_.clear();
  error_callback_.Run(CHANNEL_CONNECTION_ERROR);
}

}  // namespace protocol
}  // namespace remoting
