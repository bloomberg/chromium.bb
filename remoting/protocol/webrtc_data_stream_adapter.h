// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/message_channel_factory.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/base/refcount.h"

namespace rtc {
class PeerConnectionInterface;
}  // namespace rtc

namespace remoting {
namespace protocol {

// WebrtcDataStreamAdapter is a MessageChannelFactory that creates channels that
// send and receive messages over PeerConnection data channels.
class WebrtcDataStreamAdapter : public MessageChannelFactory {
 public:
  typedef base::Callback<void(ErrorCode)> ErrorCallback;

  explicit WebrtcDataStreamAdapter(bool outgoing,
                                   const ErrorCallback& error_callback);
  ~WebrtcDataStreamAdapter() override;

  // Initializes the adapter for |peer_connection|. If |outgoing| is set to true
  // all channels will be created as outgoing. Otherwise CreateChannel() will
  // wait for the other end to create connection.
  void Initialize(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);

  // Called by WebrtcTransport.
  void OnIncomingDataChannel(webrtc::DataChannelInterface* data_channel);

  // MessageChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  class Channel;
  friend class Channel;

  struct PendingChannel;

  void OnChannelConnected(Channel* channel);
  void OnChannelError(Channel* channel);

  const bool outgoing_;
  ErrorCallback error_callback_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  std::map<std::string, PendingChannel> pending_channels_;

  base::WeakPtrFactory<WebrtcDataStreamAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcDataStreamAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
