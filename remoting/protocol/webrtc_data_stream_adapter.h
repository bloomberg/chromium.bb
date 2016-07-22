// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/message_channel_factory.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/base/refcount.h"

namespace rtc {
class PeerConnectionInterface;
}  // namespace rtc

namespace remoting {
namespace protocol {

// WebrtcDataStreamAdapter wraps MessagePipe for WebRTC data channels.
class WebrtcDataStreamAdapter {
 public:
  typedef base::Callback<void(ErrorCode)> ErrorCallback;

  explicit WebrtcDataStreamAdapter(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);
  ~WebrtcDataStreamAdapter();

  // Creates outgoing data channel.
  std::unique_ptr<MessagePipe> CreateOutgoingChannel(const std::string& name);

  // Creates incoming data channel.
  std::unique_ptr<MessagePipe> WrapIncomingDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

 private:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcDataStreamAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
