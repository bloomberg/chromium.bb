// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_IMPL_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_IMPL_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/monitored_video_stub.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

class XmppProxy;
class VideoPacket;

namespace protocol {

class AudioReader;
class ClientControlDispatcher;
class ClientEventDispatcher;
class ClientVideoDispatcher;

class ConnectionToHostImpl : public ConnectionToHost,
                             public SignalStrategy::Listener,
                             public SessionManager::Listener,
                             public Session::EventHandler,
                             public ChannelDispatcherBase::EventHandler,
                             public base::NonThreadSafe {
 public:
  ConnectionToHostImpl();
  ~ConnectionToHostImpl() override;

  // ConnectionToHost interface.
  void set_candidate_config(scoped_ptr<CandidateSessionConfig> config) override;
  void set_client_stub(ClientStub* client_stub) override;
  void set_clipboard_stub(ClipboardStub* clipboard_stub) override;
  void set_video_stub(VideoStub* video_stub) override;
  void set_audio_stub(AudioStub* audio_stub) override;
  void Connect(SignalStrategy* signal_strategy,
               scoped_ptr<TransportFactory> transport_factory,
               scoped_ptr<Authenticator> authenticator,
               const std::string& host_jid,
               HostEventCallback* event_callback) override;
  const SessionConfig& config() override;
  ClipboardStub* clipboard_forwarder() override;
  HostStub* host_stub() override;
  InputStub* input_stub() override;
  State state() const override;

 private:
  // SignalStrategy::StatusObserver interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(const buzz::XmlElement* stanza) override;

  // SessionManager::Listener interface.
  void OnSessionManagerReady() override;
  void OnIncomingSession(
      Session* session,
      SessionManager::IncomingSessionResponse* response) override;

  // Session::EventHandler interface.
  void OnSessionStateChange(Session::State state) override;
  void OnSessionRouteChange(const std::string& channel_name,
                            const TransportRoute& route) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelError(ChannelDispatcherBase* channel_dispatcher,
                      ErrorCode error) override;

  // MonitoredVideoStub::EventHandler interface.
  virtual void OnVideoChannelStatus(bool active);

  void NotifyIfChannelsReady();

  void CloseOnError(ErrorCode error);

  // Stops writing in the channels.
  void CloseChannels();

  void SetState(State state, ErrorCode error);

  std::string host_jid_;
  std::string host_public_key_;
  scoped_ptr<Authenticator> authenticator_;

  HostEventCallback* event_callback_;

  scoped_ptr<CandidateSessionConfig> candidate_config_;

  // Stub for incoming messages.
  ClientStub* client_stub_;
  ClipboardStub* clipboard_stub_;
  AudioStub* audio_stub_;

  SignalStrategy* signal_strategy_;
  scoped_ptr<SessionManager> session_manager_;
  scoped_ptr<Session> session_;
  scoped_ptr<MonitoredVideoStub> monitored_video_stub_;

  scoped_ptr<ClientVideoDispatcher> video_dispatcher_;
  scoped_ptr<AudioReader> audio_reader_;
  scoped_ptr<ClientControlDispatcher> control_dispatcher_;
  scoped_ptr<ClientEventDispatcher> event_dispatcher_;
  ClipboardFilter clipboard_forwarder_;
  InputFilter event_forwarder_;

  // Internal state of the connection.
  State state_;
  ErrorCode error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToHostImpl);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_IMPL_H_
