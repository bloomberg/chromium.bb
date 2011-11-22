// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/proto/internal.pb.h"
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

class ClientControlDispatcher;
class ClientEventDispatcher;
class ClientStub;
class HostStub;
class InputStub;
class SessionConfig;
class VideoReader;
class VideoStub;

class ConnectionToHost : public SignalStrategy::StatusObserver,
                         public SessionManager::Listener {
 public:
  enum State {
    CONNECTING,
    // TODO(sergeyu): Currently CONNECTED state is not used and state
    // is set to AUTHENTICATED after we are connected. Remove it and
    // renamed AUTHENTICATED to CONNECTED?
    CONNECTED,
    AUTHENTICATED,
    FAILED,
    CLOSED,
  };

  enum Error {
    OK,
    HOST_IS_OFFLINE,
    SESSION_REJECTED,
    INCOMPATIBLE_PROTOCOL,
    NETWORK_FAILURE,
  };

  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Called when state of the connection changes.
    virtual void OnConnectionState(State state, Error error) = 0;
  };

  ConnectionToHost(base::MessageLoopProxy* message_loop,
                   pp::Instance* pp_instance,
                   bool allow_nat_traversal);
  virtual ~ConnectionToHost();

  virtual void Connect(scoped_refptr<XmppProxy> xmpp_proxy,
                       const std::string& your_jid,
                       const std::string& host_jid,
                       const std::string& host_public_key,
                       const std::string& access_code,
                       HostEventCallback* event_callback,
                       ClientStub* client_stub,
                       VideoStub* video_stub);

  virtual void Disconnect(const base::Closure& shutdown_task);

  virtual const SessionConfig& config();

  virtual InputStub* input_stub();

  virtual HostStub* host_stub();

  // SignalStrategy::StatusObserver interface.
  virtual void OnStateChange(
      SignalStrategy::StatusObserver::State state) OVERRIDE;
  virtual void OnJidChange(const std::string& full_jid) OVERRIDE;

  // SessionManager::Listener interface.
  virtual void OnSessionManagerInitialized() OVERRIDE;
  virtual void OnIncomingSession(
      Session* session,
      SessionManager::IncomingSessionResponse* response) OVERRIDE;

  // Called when the host accepts the client authentication.
  void OnClientAuthenticated();

  // Return the current state of ConnectionToHost.
  State state() const;

 private:
  // Called on the jingle thread after we've successfully to XMPP server. Starts
  // P2P connection to the host.
  void InitSession();

  // Callback for |session_|.
  void OnSessionStateChange(Session::State state);

  // Callbacks for channel initialization
  void OnChannelInitialized(bool successful);

  void NotifyIfChannelsReady();

  // Callback for |video_reader_|.
  void OnVideoPacket(VideoPacket* packet);

  void CloseOnError(Error error);

  // Stops writing in the channels.
  void CloseChannels();

  void SetState(State state, Error error);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  pp::Instance* pp_instance_;
  bool allow_nat_traversal_;

  std::string host_jid_;
  std::string host_public_key_;
  std::string access_code_;

  HostEventCallback* event_callback_;

  // Stub for incoming messages.
  ClientStub* client_stub_;
  VideoStub* video_stub_;

  scoped_ptr<SignalStrategy> signal_strategy_;
  std::string local_jid_;
  scoped_ptr<SessionManager> session_manager_;
  scoped_ptr<Session> session_;

  scoped_ptr<VideoReader> video_reader_;
  scoped_ptr<ClientControlDispatcher> control_dispatcher_;
  scoped_ptr<ClientEventDispatcher> event_dispatcher_;

  // Internal state of the connection.
  State state_;
  Error error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToHost);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
