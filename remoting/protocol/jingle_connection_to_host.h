// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// JingleConnectionToHost implements the ConnectionToHost interface using
// libjingle as the transport protocol.
//
// Much of this class focuses on translating JingleClient and
// ChromotingConnection callbacks into ConnectionToHost::HostEventCallback
// messages.
//
// The public API of this class is designed to be asynchronous, and thread
// safe for invocation from other threads.
//
// Internally though, all work delegeated to the |network_thread| given
// during construction.  Any event handlers running on the |network_thread|
// should not block.

#ifndef REMOTING_PROTOCOL_JINGLE_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_JINGLE_CONNECTION_TO_HOST_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"

class MessageLoop;

namespace remoting {

class JingleThread;
class VideoPacket;

namespace protocol {

class ClientMessageDispatcher;
class VideoReader;
class VideoStub;

class JingleConnectionToHost : public ConnectionToHost,
                               public JingleClient::Callback {
 public:
  // TODO(sergeyu): Constructor shouldn't need thread here.
  explicit JingleConnectionToHost(JingleThread* thread);
  virtual ~JingleConnectionToHost();

  virtual void Connect(const std::string& username,
                       const std::string& auth_token,
                       const std::string& host_jid,
                       HostEventCallback* event_callback,
                       ClientStub* client_stub,
                       VideoStub* video_stub);
  virtual void Disconnect();

  virtual const SessionConfig* config();

  // JingleClient::Callback interface.
  virtual void OnStateChange(JingleClient* client, JingleClient::State state);

  // Callback for chromotocol SessionManager.
  void OnNewSession(
      Session* connection,
      SessionManager::IncomingSessionResponse* response);

  // Callback for chromotocol Session.
  void OnSessionStateChange(Session::State state);

 private:
  // The message loop for the jingle thread this object works on.
  MessageLoop* message_loop();

  // Called on the jingle thread after we've successfully to XMPP server. Starts
  // P2P connection to the host.
  void InitSession();

  // Callback for |video_reader_|.
  void OnVideoPacket(VideoPacket* packet);

  // Used by Disconnect() to disconnect chromoting connection, stop chromoting
  // server, and then disconnect XMPP connection.
  void OnDisconnected();
  void OnServerClosed();

  JingleThread* thread_;

  scoped_refptr<JingleClient> jingle_client_;
  scoped_refptr<SessionManager> session_manager_;
  scoped_refptr<Session> session_;

  scoped_ptr<VideoReader> video_reader_;

  HostEventCallback* event_callback_;
  VideoStub* video_stub_;

  std::string host_jid_;

  scoped_ptr<ClientMessageDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(JingleConnectionToHost);
};

}  // namespace protocol
}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::protocol::JingleConnectionToHost);

#endif  // REMOTING_PROTOCOL_JINGLE_CONNECTION_TO_HOST_H_
