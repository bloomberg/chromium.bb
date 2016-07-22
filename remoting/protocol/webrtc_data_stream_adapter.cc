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

namespace {

class WebrtcDataChannel : public MessagePipe,
                          public webrtc::DataChannelObserver {
 public:
  explicit WebrtcDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel);
  ~WebrtcDataChannel() override;

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

  rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;

  EventHandler* event_handler_ = nullptr;

  State state_ = State::CONNECTING;

  DISALLOW_COPY_AND_ASSIGN(WebrtcDataChannel);
};

WebrtcDataChannel::WebrtcDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
    : channel_(channel) {
  channel_->RegisterObserver(this);
  DCHECK_EQ(channel_->state(), webrtc::DataChannelInterface::kConnecting);
}

WebrtcDataChannel::~WebrtcDataChannel() {
  if (channel_) {
    channel_->UnregisterObserver();
    channel_->Close();
  }
}

void WebrtcDataChannel::Start(EventHandler* event_handler) {
  DCHECK(!event_handler_);
  DCHECK(event_handler);

  event_handler_ = event_handler;
}

void WebrtcDataChannel::Send(google::protobuf::MessageLite* message,
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

void WebrtcDataChannel::OnStateChange() {
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kOpen:
      DCHECK(state_ == State::CONNECTING);
      state_ = State::OPEN;
      event_handler_->OnMessagePipeOpen();
      break;

    case webrtc::DataChannelInterface::kClosing:
      if (state_ != State::CLOSED) {
        state_ = State::CLOSED;
        event_handler_->OnMessagePipeClosed();
      }
      break;

    case webrtc::DataChannelInterface::kConnecting:
    case webrtc::DataChannelInterface::kClosed:
      break;
  }
}

void WebrtcDataChannel::OnMessage(const webrtc::DataBuffer& rtc_buffer) {
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

}  // namespace

WebrtcDataStreamAdapter::WebrtcDataStreamAdapter(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection)
    : peer_connection_(peer_connection) {}
WebrtcDataStreamAdapter::~WebrtcDataStreamAdapter() {}

std::unique_ptr<MessagePipe> WebrtcDataStreamAdapter::CreateOutgoingChannel(
    const std::string& name) {
  webrtc::DataChannelInit config;
  config.reliable = true;
  return base::WrapUnique(new WebrtcDataChannel(
      peer_connection_->CreateDataChannel(name, &config)));
}

std::unique_ptr<MessagePipe> WebrtcDataStreamAdapter::WrapIncomingDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  return base::WrapUnique(new WebrtcDataChannel(data_channel));
}

}  // namespace protocol
}  // namespace remoting
