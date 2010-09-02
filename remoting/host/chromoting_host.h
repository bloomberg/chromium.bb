// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CHROMOTING_HOST_H_
#define REMOTING_CHROMOTING_HOST_H_

#include <string>

#include "base/thread.h"
#include "remoting/base/encoder.h"
#include "remoting/host/access_verifier.h"
#include "remoting/host/capturer.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"

class Task;

namespace remoting {

class Capturer;
class ChromotingHostContext;
class Encoder;
class EventExecutor;
class MutableHostConfig;
class SessionManager;

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
  ChromotingHost(ChromotingHostContext* context, MutableHostConfig* config,
                 Capturer* capturer, Encoder* encoder, EventExecutor* executor);
  virtual ~ChromotingHost();

  // Start the host process. This method starts the chromoting host
  // asynchronously.
  //
  // |shutdown_task| is called if Start() has failed ot Shutdown() is called
  // and all related operations are completed.
  //
  // This method can only be called once during the lifetime of this object.
  void Start(Task* shutdown_task);

  // This method is called when we need to destroy the host process.
  void Shutdown();

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
  enum State {
    kInitial,
    kStarted,
    kStopped,
  };

  // This method connects to the talk network and start listening for incoming
  // connections.
  void DoStart(Task* shutdown_task);

  // This method shuts down the host process.
  void DoShutdown();

  // The context that the chromoting host runs on.
  ChromotingHostContext* context_;

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

  AccessVerifier access_verifier_;

  // A ClientConnection manages the connectino to a remote client.
  // TODO(hclam): Expand this to a list of clients.
  scoped_refptr<ClientConnection> client_;

  // Session manager for the host process.
  scoped_refptr<SessionManager> session_;

  // This task gets executed when this object fails to connect to the
  // talk network or Shutdown() is called.
  scoped_ptr<Task> shutdown_task_;

  // Tracks the internal state of the host.
  // This variable is written on the main thread of ChromotingHostContext
  // and read by jingle thread.
  State state_;

  // Lock is to lock the access to |state_|.
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_H_
