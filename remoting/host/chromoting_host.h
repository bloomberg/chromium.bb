// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CHROMOTING_HOST_H_
#define REMOTING_CHROMOTING_HOST_H_

#include <string>

#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "remoting/base/encoder.h"
#include "remoting/host/access_verifier.h"
#include "remoting/host/capturer.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/connection_to_client.h"

class Task;

namespace remoting {

namespace protocol {
class ConnectionToClient;
class HostStub;
class InputStub;
class SessionConfig;
class CandidateSessionConfig;
}  // namespace protocol

class Capturer;
class ChromotingHostContext;
class DesktopEnvironment;
class Encoder;
class MutableHostConfig;
class ScreenRecorder;

// A class to implement the functionality of a host process.
//
// Here's the work flow of this class:
// 1. We should load the saved GAIA ID token or if this is the first
//    time the host process runs we should prompt user for the
//    credential. We will use this token or credentials to authenicate
//    and register the host.
//
// 2. We listen for incoming connection using libjingle. We will create
//    a ConnectionToClient object that wraps around linjingle for transport.
//    A ScreenRecorder is created with an Encoder and a Capturer.
//    A ConnectionToClient is added to the ScreenRecorder for transporting
//    the screen captures. An InputStub is created and registered with the
//    ConnectionToClient to receive mouse / keyboard events from the remote
//    client.
//    After we have done all the initialization we'll start the ScreenRecorder.
//    We'll then enter the running state of the host process.
//
// 3. When the user is disconnected, we will pause the ScreenRecorder
//    and try to terminate the threads we have created. This will allow
//    all pending tasks to complete. After all of that completed we
//    return to the idle state. We then go to step (2) if there a new
//    incoming connection.
class ChromotingHost : public base::RefCountedThreadSafe<ChromotingHost>,
                       public protocol::ConnectionToClient::EventHandler,
                       public DesktopEnvironment::EventHandler,
                       public JingleClient::Callback {
 public:
  // Factory methods that must be used to create ChromotingHost instances.
  // Default capturer and input stub are used if it is not specified.
  static ChromotingHost* Create(ChromotingHostContext* context,
                                MutableHostConfig* config);
  static ChromotingHost* Create(ChromotingHostContext* context,
                                MutableHostConfig* config,
                                DesktopEnvironment* environment);

  // Asynchronously start the host process.
  //
  // After this is invoked, the host process will connect to the talk
  // network and start listening for incoming connections.
  //
  // |shutdown_task| is called if Start() has failed ot Shutdown() is called
  // and all related operations are completed.
  //
  // This method can only be called once during the lifetime of this object.
  void Start(Task* shutdown_task);

  // Asynchronously shutdown the host process.
  void Shutdown();

  // This method is called if a client is connected to this object.
  void OnClientConnected(protocol::ConnectionToClient* client);

  // This method is called if a client is disconnected from the host.
  void OnClientDisconnected(protocol::ConnectionToClient* client);

  ////////////////////////////////////////////////////////////////////////////
  // protocol::ConnectionToClient::EventHandler implementations
  virtual void OnConnectionOpened(protocol::ConnectionToClient* client);
  virtual void OnConnectionClosed(protocol::ConnectionToClient* client);
  virtual void OnConnectionFailed(protocol::ConnectionToClient* client);

  ////////////////////////////////////////////////////////////////////////////
  // JingleClient::Callback implementations
  virtual void OnStateChange(JingleClient* client, JingleClient::State state);

  ////////////////////////////////////////////////////////////////////////////
  // DesktopEnvironment::EventHandler implementations
  virtual void LocalLoginSucceeded();
  virtual void LocalLoginFailed();

  // Callback for ChromotingServer.
  void OnNewClientSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response);

  // Sets desired configuration for the protocol. Ownership of the
  // |config| is transferred to the object. Must be called before Start().
  void set_protocol_config(protocol::CandidateSessionConfig* config);

  // This getter is only used in unit test.
  protocol::HostStub* host_stub() const;

  // This setter is only used in unit test to simulate client connection.
  void set_connection(protocol::ConnectionToClient* conn) {
    connection_ = conn;
  }

 private:
  friend class base::RefCountedThreadSafe<ChromotingHost>;

  ChromotingHost(ChromotingHostContext* context, MutableHostConfig* config,
                 DesktopEnvironment* environment);
  virtual ~ChromotingHost();

  enum State {
    kInitial,
    kStarted,
    kStopped,
  };

  // Callback for protocol::SessionManager::Close().
  void OnServerClosed();

  // Creates encoder for the specified configuration.
  Encoder* CreateEncoder(const protocol::SessionConfig* config);

  std::string GenerateHostAuthToken(const std::string& encoded_client_token);

  // The context that the chromoting host runs on.
  ChromotingHostContext* context_;

  scoped_refptr<MutableHostConfig> config_;

  scoped_ptr<DesktopEnvironment> desktop_environment_;

  scoped_ptr<SignalStrategy> signal_strategy_;

  // The libjingle client. This is used to connect to the talk network to
  // receive connection requests from chromoting client.
  scoped_refptr<JingleClient> jingle_client_;

  scoped_refptr<protocol::SessionManager> session_manager_;

  // Objects that takes care of sending heartbeats to the chromoting bot.
  scoped_refptr<HeartbeatSender> heartbeat_sender_;

  AccessVerifier access_verifier_;

  // A ConnectionToClient manages the connectino to a remote client.
  // TODO(hclam): Expand this to a list of clients.
  scoped_refptr<protocol::ConnectionToClient> connection_;

  // Session manager for the host process.
  scoped_refptr<ScreenRecorder> recorder_;

  // This task gets executed when this object fails to connect to the
  // talk network or Shutdown() is called.
  scoped_ptr<Task> shutdown_task_;

  // Tracks the internal state of the host.
  // This variable is written on the main thread of ChromotingHostContext
  // and read by jingle thread.
  State state_;

  // Lock is to lock the access to |state_|.
  base::Lock lock_;

  // Configuration of the protocol.
  scoped_ptr<protocol::CandidateSessionConfig> protocol_config_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_H_
