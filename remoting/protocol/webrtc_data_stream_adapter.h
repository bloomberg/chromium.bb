// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/stream_channel_factory.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/webrtc/base/refcount.h"

namespace rtc {
class PeerConnectionInterface;
}  // namespace rtc

namespace remoting {
namespace protocol {

class WebrtcDataStreamAdapter : public StreamChannelFactory {
 public:
  WebrtcDataStreamAdapter();
  ~WebrtcDataStreamAdapter() override;

  void Initialize(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection,
      bool is_server);

  void OnIncomingDataChannel(webrtc::DataChannelInterface* data_channel);

  // StreamChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  class Channel;

  void OnChannelConnected(const ChannelCreatedCallback& connected_callback,
                          Channel* channel,
                          bool connected);

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  bool is_server_ = false;

  std::map<std::string, Channel*> pending_channels_;

  base::WeakPtrFactory<WebrtcDataStreamAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcDataStreamAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_DATA_STREAM_ADAPTER_H_
