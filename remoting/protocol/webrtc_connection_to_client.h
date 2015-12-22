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

namespace remoting {
namespace protocol {

class HostControlDispatcher;
class HostEventDispatcher;

class WebrtcConnectionToClient : public ConnectionToClient,
                                 public Session::EventHandler,
                                 public ChannelDispatcherBase::EventHandler {
 public:
  explicit WebrtcConnectionToClient(scoped_ptr<Session> session);
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
  void OnSessionRouteChange(const std::string& channel_name,
                            const TransportRoute& route) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelError(ChannelDispatcherBase* channel_dispatcher,
                      ErrorCode error) override;

 private:
  base::ThreadChecker thread_checker_;

  // Event handler for handling events sent from this object.
  ConnectionToClient::EventHandler* event_handler_ = nullptr;

  scoped_ptr<Session> session_;

  scoped_ptr<HostControlDispatcher> control_dispatcher_;
  scoped_ptr<HostEventDispatcher> event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcConnectionToClient);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_CLIENT_H_
