// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/webrtc_transport.h"

namespace remoting {
namespace protocol {

class HostControlDispatcher;
class HostEventDispatcher;

class WebrtcConnectionToClient : public ConnectionToClient,
                                 public Session::EventHandler,
                                 public WebrtcTransport::EventHandler,
                                 public ChannelDispatcherBase::EventHandler {
 public:
  WebrtcConnectionToClient(
      scoped_ptr<Session> session,
      scoped_refptr<protocol::TransportContext> transport_context);
  ~WebrtcConnectionToClient() override;

  // ConnectionToClient interface.
  void SetEventHandler(
      ConnectionToClient::EventHandler* event_handler) override;
  Session* session() override;
  void Disconnect(ErrorCode error) override;
  void OnInputEventReceived(int64_t timestamp) override;
  scoped_ptr<VideoStream> StartVideoStream(
      scoped_ptr<webrtc::DesktopCapturer> desktop_capturer) override;
  AudioStub* audio_stub() override;
  ClientStub* client_stub() override;
  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_host_stub(HostStub* host_stub) override;
  void set_input_stub(InputStub* input_stub) override;

  // Session::EventHandler interface.
  void OnSessionStateChange(Session::State state) override;

  // WebrtcTransport::EventHandler interface
  void OnWebrtcTransportConnecting() override;
  void OnWebrtcTransportConnected() override;
  void OnWebrtcTransportError(ErrorCode error) override;
  void OnWebrtcTransportMediaStreamAdded(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnWebrtcTransportMediaStreamRemoved(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;

 private:
  base::ThreadChecker thread_checker_;

  // Event handler for handling events sent from this object.
  ConnectionToClient::EventHandler* event_handler_ = nullptr;

  WebrtcTransport transport_;

  scoped_ptr<Session> session_;

  scoped_ptr<HostControlDispatcher> control_dispatcher_;
  scoped_ptr<HostEventDispatcher> event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcConnectionToClient);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_
