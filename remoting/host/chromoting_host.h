// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CHROMOTING_HOST_H_
#define REMOTING_CHROMOTING_HOST_H_

#include <string>

#include "base/thread.h"
#include "remoting/host/capturer.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/encoder.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/session_manager.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace remoting {

class MutableHostConfig;

// A class to implement the functionality of a host process.
//
// Here's the work flow of this class:
// 1. We should load the saved GAIA ID token or if this is the first
//    time the host process runs we should prompt user for the
//    credential. We will use this token or credentials to authenicate
//    and register the host.
//
// 2. We listen for incoming connection using libjingle. We will create
//    a ClientConnection object that wraps around linjingle for transport. Also
//    create a SessionManager with appropriate Encoder and Capturer and
//    add the ClientConnection to this SessionManager for transporting the
//    screen captures. A EventExecutor is created and registered with the
//    ClientConnection to receive mouse / keyboard events from the remote
//    client.
//    This is also the right time to create multiple threads to host
//    the above objects. After we have done all the initialization
//    we'll start the SessionManager. We'll then enter the running state
//    of the host process.
//
// 3. When the user is disconencted, we will pause the SessionManager
//    and try to terminate the threads we have created. This will allow
//    all pending tasks to complete. After all of that completed we
//    return to the idle state. We then go to step (2) if there a new
//    incoming connection.
class ChromotingHost : public base::RefCountedThreadSafe<ChromotingHost>,
                   public ClientConnection::EventHandler,
                   public JingleClient::Callback {
 public:
  ChromotingHost(MutableHostConfig* config, Capturer* capturer,
                 Encoder* encoder, EventExecutor* executor,
                 base::WaitableEvent* host_done);
  virtual ~ChromotingHost();

  // Run the host porcess. This method returns only after the message loop
  // of the host process exits.
  void Run();

  // This method is called when we need to the host process.
  void DestroySession();

  // This method talks to the cloud to register the host process. If
  // successful we will start listening to network requests.
  void RegisterHost();

  // This method is called if a client is connected to this object.
  void OnClientConnected(ClientConnection* client);

  // This method is called if a client is disconnected from the host.
  void OnClientDisconnected(ClientConnection* client);

  ////////////////////////////////////////////////////////////////////////////
  // ClientConnection::EventHandler implementations
  virtual void HandleMessages(ClientConnection* client,
                              ClientMessageList* messages);
  virtual void OnConnectionOpened(ClientConnection* client);
  virtual void OnConnectionClosed(ClientConnection* client);
  virtual void OnConnectionFailed(ClientConnection* client);

  ////////////////////////////////////////////////////////////////////////////
  // JingleClient::Callback implementations
  virtual void OnStateChange(JingleClient* client, JingleClient::State state);
  virtual bool OnAcceptConnection(
      JingleClient* jingle, const std::string& jid,
      JingleChannel::Callback** channel_callback);
  virtual void OnNewConnection(
      JingleClient* jingle,
      scoped_refptr<JingleChannel> channel);

 private:
  // The message loop that this class runs on.
  MessageLoop* message_loop();

  // The main thread that this object runs on.
  base::Thread main_thread_;

  // Used to handle the Jingle connection.
  JingleThread network_thread_;

  // A thread that hosts capture operations.
  base::Thread capture_thread_;

  // A thread that hosts encode operations.
  base::Thread encode_thread_;

  scoped_refptr<MutableHostConfig> config_;

  // Capturer to be used by SessionManager. Once the SessionManager is
  // constructed this is set to NULL.
  scoped_ptr<Capturer> capturer_;

  // Encoder to be used by the SessionManager. Once the SessionManager is
  // constructed this is set to NULL.
  scoped_ptr<Encoder> encoder_;

  // EventExecutor executes input events received from the client.
  scoped_ptr<EventExecutor> executor_;

  // The libjingle client. This is used to connect to the talk network to
  // receive connection requests from chromoting client.
  scoped_refptr<JingleClient> jingle_client_;

  // Objects that takes care of sending heartbeats to the chromoting bot.
  scoped_refptr<HeartbeatSender> heartbeat_sender_;

  // A ClientConnection manages the connectino to a remote client.
  // TODO(hclam): Expand this to a list of clients.
  scoped_refptr<ClientConnection> client_;

  // Session manager for the host process.
  scoped_refptr<SessionManager> session_;

  // Signals the host is ready to be destroyed.
  base::WaitableEvent* host_done_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_H_
