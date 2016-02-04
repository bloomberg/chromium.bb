// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_HOST_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/webrtc_transport.h"

namespace remoting {
namespace protocol {

class ClientControlDispatcher;
class ClientEventDispatcher;
class SessionConfig;
class WebrtcVideoRendererAdapter;

class WebrtcConnectionToHost : public ConnectionToHost,
                               public Session::EventHandler,
                               public WebrtcTransport::EventHandler,
                               public ChannelDispatcherBase::EventHandler {
 public:
  WebrtcConnectionToHost();
  ~WebrtcConnectionToHost() override;

  // ConnectionToHost interface.
  void set_client_stub(ClientStub* client_stub) override;
  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_video_renderer(VideoRenderer* video_renderer) override;
  void set_audio_stub(AudioStub* audio_stub) override;
  void Connect(scoped_ptr<Session> session,
               scoped_refptr<TransportContext> transport_context,
               HostEventCallback* event_callback) override;
  const SessionConfig& config() override;
  ClipboardStub* clipboard_forwarder() override;
  HostStub* host_stub() override;
  InputStub* input_stub() override;
  State state() const override;

 private:
  // Session::EventHandler interface.
  void OnSessionStateChange(Session::State state) override;

  // WebrtcTransport::EventHandler interface.
  void OnWebrtcTransportConnecting() override;
  void OnWebrtcTransportConnected() override;
  void OnWebrtcTransportError(ErrorCode error) override;
  void OnWebrtcTransportMediaStreamAdded(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnWebrtcTransportMediaStreamRemoved(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;

  void NotifyIfChannelsReady();

  void CloseChannels();

  void SetState(State state, ErrorCode error);

  HostEventCallback* event_callback_ = nullptr;

  // Stub for incoming messages.
  ClientStub* client_stub_ = nullptr;
  VideoRenderer* video_renderer_ = nullptr;
  ClipboardStub* clipboard_stub_ = nullptr;

  scoped_ptr<Session> session_;
  scoped_ptr<WebrtcTransport> transport_;

  scoped_ptr<ClientControlDispatcher> control_dispatcher_;
  scoped_ptr<ClientEventDispatcher> event_dispatcher_;
  ClipboardFilter clipboard_forwarder_;
  InputFilter event_forwarder_;

  scoped_ptr<WebrtcVideoRendererAdapter> video_adapter_;

  // Internal state of the connection.
  State state_ = INITIALIZING;
  ErrorCode error_ = OK;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebrtcConnectionToHost);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_CONNECTION_TO_HOST_H_
