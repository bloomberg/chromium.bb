// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"

class MessageLoop;

namespace remoting {

class JingleThread;
class VideoPacket;

namespace protocol {

class ClientMessageDispatcher;
class ClientStub;
class SessionConfig;
class VideoReader;
class VideoStub;

class ConnectionToHost : public JingleClient::Callback {
 public:
  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Called when the network connection is opened.
    virtual void OnConnectionOpened(ConnectionToHost* conn) = 0;

    // Called when the network connection is closed.
    virtual void OnConnectionClosed(ConnectionToHost* conn) = 0;

    // Called when the network connection has failed.
    virtual void OnConnectionFailed(ConnectionToHost* conn) = 0;
  };

  // Takes ownership of |network_manager| and |socket_factory|. Both
  // |network_manager| and |socket_factory| may be set to NULL.
  //
  // TODO(sergeyu): Constructor shouldn't need thread here.
  ConnectionToHost(JingleThread* thread,
                   talk_base::NetworkManager* network_manager,
                   talk_base::PacketSocketFactory* socket_factory);
  virtual ~ConnectionToHost();

  // TODO(ajwong): We need to generalize this API.
  virtual void Connect(const std::string& username,
                       const std::string& auth_token,
                       const std::string& host_jid,
                       HostEventCallback* event_callback,
                       ClientStub* client_stub,
                       VideoStub* video_stub);
  virtual void Disconnect();

  virtual const SessionConfig* config();

  virtual InputStub* input_stub();

  virtual HostStub* host_stub();

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

  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;

  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_refptr<JingleClient> jingle_client_;
  scoped_refptr<SessionManager> session_manager_;
  scoped_refptr<Session> session_;

  scoped_ptr<VideoReader> video_reader_;

  HostEventCallback* event_callback_;

  std::string host_jid_;

  scoped_ptr<ClientMessageDispatcher> dispatcher_;

  ////////////////////////////////////////////////////////////////////////////
  // User input event channel interface

  // Stub for sending input event messages to the host.
  scoped_ptr<InputStub> input_stub_;

  ////////////////////////////////////////////////////////////////////////////
  // Protocol control channel interface

  // Stub for sending control messages to the host.
  scoped_ptr<HostStub> host_stub_;

  // Stub for receiving control messages from the host.
  ClientStub* client_stub_;

  ////////////////////////////////////////////////////////////////////////////
  // Video channel interface

  // Stub for receiving video packets from the host.
  VideoStub* video_stub_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToHost);
};

}  // namespace protocol
}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::protocol::ConnectionToHost);

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
