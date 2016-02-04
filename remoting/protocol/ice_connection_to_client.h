// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_ICE_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/ice_transport.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

class AudioWriter;
class HostControlDispatcher;
class HostEventDispatcher;
class HostVideoDispatcher;
class VideoFeedbackStub;
class VideoFramePump;

// This class represents a remote viewer connection to the chromoting
// host. It sets up all protocol channels and connects them to the
// stubs.
class IceConnectionToClient : public ConnectionToClient,
                              public Session::EventHandler,
                              public IceTransport::EventHandler,
                              public ChannelDispatcherBase::EventHandler {
 public:
  IceConnectionToClient(
      scoped_ptr<Session> session,
      scoped_refptr<TransportContext> transport_context,
      scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner);
  ~IceConnectionToClient() override;

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

  // IceTransport::EventHandler interface.
  void OnIceTransportRouteChange(const std::string& channel_name,
                                 const TransportRoute& route) override;
  void OnIceTransportError(ErrorCode error) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;

 private:
  void NotifyIfChannelsReady();

  void CloseChannels();

  base::ThreadChecker thread_checker_;

  // Event handler for handling events sent from this object.
  ConnectionToClient::EventHandler* event_handler_;

  scoped_ptr<Session> session_;

  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;

  IceTransport transport_;

  scoped_ptr<HostControlDispatcher> control_dispatcher_;
  scoped_ptr<HostEventDispatcher> event_dispatcher_;
  scoped_ptr<HostVideoDispatcher> video_dispatcher_;
  scoped_ptr<AudioWriter> audio_writer_;

  DISALLOW_COPY_AND_ASSIGN(IceConnectionToClient);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_CONNECTION_TO_CLIENT_H_
