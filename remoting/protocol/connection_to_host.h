// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/clipboard_filter.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/input_filter.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace pp {
class Instance;
}  // namespace pp

namespace remoting {

class XmppProxy;
class VideoPacket;

namespace protocol {

class Authenticator;
class ClientControlDispatcher;
class ClientEventDispatcher;
class ClientStub;
class ClipboardStub;
class HostStub;
class InputStub;
class SessionConfig;
class TransportFactory;
class VideoReader;
class VideoStub;

class ConnectionToHost : public SignalStrategy::Listener,
                         public SessionManager::Listener {
 public:
  enum State {
    CONNECTING,
    CONNECTED,
    FAILED,
    CLOSED,
  };

  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Called when state of the connection changes.
    virtual void OnConnectionState(State state, ErrorCode error) = 0;
  };

  ConnectionToHost(base::MessageLoopProxy* message_loop,
                   bool allow_nat_traversal);
  virtual ~ConnectionToHost();

  virtual void Connect(scoped_refptr<XmppProxy> xmpp_proxy,
                       const std::string& local_jid,
                       const std::string& host_jid,
                       const std::string& host_public_key,
                       scoped_ptr<TransportFactory> transport_factory,
                       scoped_ptr<Authenticator> authenticator,
                       HostEventCallback* event_callback,
                       ClientStub* client_stub,
                       ClipboardStub* clipboard_stub,
                       VideoStub* video_stub);

  virtual void Disconnect(const base::Closure& shutdown_task);

  virtual const SessionConfig& config();

  virtual ClipboardStub* clipboard_stub();
  virtual HostStub* host_stub();
  virtual InputStub* input_stub();

  // SignalStrategy::StatusObserver interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;

  // SessionManager::Listener interface.
  virtual void OnSessionManagerReady() OVERRIDE;
  virtual void OnIncomingSession(
      Session* session,
      SessionManager::IncomingSessionResponse* response) OVERRIDE;

  // Called when the host accepts the client authentication.
  void OnClientAuthenticated();

  // Return the current state of ConnectionToHost.
  State state() const;

 private:
  // Callback for |session_|.
  void OnSessionStateChange(Session::State state);

  // Callbacks for channel initialization
  void OnChannelInitialized(bool successful);

  void NotifyIfChannelsReady();

  // Callback for |video_reader_|.
  void OnVideoPacket(VideoPacket* packet);

  void CloseOnError(ErrorCode error);

  // Stops writing in the channels.
  void CloseChannels();

  void SetState(State state, ErrorCode error);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  bool allow_nat_traversal_;

  std::string host_jid_;
  std::string host_public_key_;
  scoped_ptr<Authenticator> authenticator_;

  HostEventCallback* event_callback_;

  // Stub for incoming messages.
  ClientStub* client_stub_;
  ClipboardStub* clipboard_stub_;
  VideoStub* video_stub_;

  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<SessionManager> session_manager_;
  scoped_ptr<Session> session_;

  scoped_ptr<VideoReader> video_reader_;
  scoped_ptr<ClientControlDispatcher> control_dispatcher_;
  scoped_ptr<ClientEventDispatcher> event_dispatcher_;
  ClipboardFilter clipboard_forwarder_;
  InputFilter event_forwarder_;

  // Internal state of the connection.
  State state_;
  ErrorCode error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToHost);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
