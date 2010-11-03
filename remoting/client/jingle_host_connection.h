// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// JingleHostConnection implements the HostConnection interface using
// libjingle as the transport protocol.
//
// Much of this class focuses on translating JingleClient and
// ChromotingConnection callbacks into HostConnection::HostEventCallback
// messages.
//
// The public API of this class is designed to be asynchronous, and thread
// safe for invocation from other threads.
//
// Internally though, all work delegeated to the |network_thread| given
// during construction.  Any event handlers running on the |network_thread|
// should not block.

#ifndef REMOTING_CLIENT_JINGLE_HOST_CONNECTION_H_
#define REMOTING_CLIENT_JINGLE_HOST_CONNECTION_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "remoting/client/client_context.h"
#include "remoting/client/host_connection.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/chromotocol_connection.h"
#include "remoting/protocol/chromotocol_server.h"
#include "remoting/protocol/stream_writer.h"
#include "remoting/protocol/video_reader.h"

class MessageLoop;

namespace remoting {

class JingleThread;
class VideoStub;

struct ClientConfig;

class JingleHostConnection : public HostConnection,
                             public JingleClient::Callback {
 public:
  explicit JingleHostConnection(ClientContext* context);
  virtual ~JingleHostConnection();

  virtual void Connect(const ClientConfig& config,
                       HostEventCallback* event_callback,
                       VideoStub* video_stub);
  virtual void Disconnect();

  virtual void SendEvent(const ChromotingClientMessage& msg);

  // JingleClient::Callback interface.
  virtual void OnStateChange(JingleClient* client, JingleClient::State state);

  // Callback for ChromotocolServer.
  void OnNewChromotocolConnection(
      ChromotocolConnection* connection,
      ChromotocolServer::IncomingConnectionResponse* response);

  // Callback for ChromotocolConnection.
  void OnConnectionStateChange(ChromotocolConnection::State state);

 private:
  // The message loop for the jingle thread this object works on.
  MessageLoop* message_loop();

  // Called on the jingle thread after we've successfully to XMPP server. Starts
  // P2P connection to the host.
  void InitConnection();

  // Callback for |control_reader_|.
  void OnControlMessage(ChromotingHostMessage* msg);

  // Callback for |video_reader_|.
  void OnVideoPacket(VideoPacket* packet);

  // Used by Disconnect() to disconnect chromoting connection, stop chromoting
  // server, and then disconnect XMPP connection.
  void OnDisconnected();
  void OnServerClosed();

  ClientContext* context_;

  scoped_refptr<JingleClient> jingle_client_;
  scoped_refptr<ChromotocolServer> chromotocol_server_;
  scoped_refptr<ChromotocolConnection> connection_;

  MessageReader control_reader_;
  EventStreamWriter event_writer_;
  scoped_ptr<VideoReader> video_reader_;

  HostEventCallback* event_callback_;
  VideoStub* video_stub_;

  std::string host_jid_;

  DISALLOW_COPY_AND_ASSIGN(JingleHostConnection);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::JingleHostConnection);

#endif  // REMOTING_CLIENT_JINGLE_HOST_CONNECTION_H_
