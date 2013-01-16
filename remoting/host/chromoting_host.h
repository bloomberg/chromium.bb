// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_H_
#define REMOTING_HOST_CHROMOTING_HOST_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "net/base/backoff_entry.h"
#include "remoting/host/client_session.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/mouse_move_observer.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/connection_to_client.h"
#include "third_party/skia/include/core/SkSize.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class InputStub;
class SessionConfig;
class CandidateSessionConfig;
}  // namespace protocol

class DesktopEnvironmentFactory;

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
//    A ScreenRecorder is created with an Encoder and a VideoFrameCapturer.
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
                       public ClientSession::EventHandler,
                       public protocol::SessionManager::Listener,
                       public MouseMoveObserver {
 public:
  // The caller must ensure that |signal_strategy| and
  // |desktop_environment_factory| remain valid at least until the
  // |shutdown_task| supplied to Shutdown() has been notified.
  ChromotingHost(
      SignalStrategy* signal_strategy,
      DesktopEnvironmentFactory* desktop_environment_factory,
      scoped_ptr<protocol::SessionManager> session_manager,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Asynchronously start the host process.
  //
  // After this is invoked, the host process will connect to the talk
  // network and start listening for incoming connections.
  //
  // This method can only be called once during the lifetime of this object.
  void Start(const std::string& xmpp_login);

  // Asynchronously shutdown the host process. |shutdown_task| is
  // called after shutdown is completed.
  void Shutdown(const base::Closure& shutdown_task);

  // Add/Remove |observer| to/from the list of status observers. Both
  // methods can be called on the network thread only.
  void AddStatusObserver(HostStatusObserver* observer);
  void RemoveStatusObserver(HostStatusObserver* observer);

  // This method may be called only from
  // HostStatusObserver::OnClientAuthenticated() to reject the new
  // client.
  void RejectAuthenticatingClient();

  // Sets the authenticator factory to use for incoming
  // connections. Incoming connections are rejected until
  // authenticator factory is set. Must be called on the network
  // thread after the host is started. Must not be called more than
  // once per host instance because it may not be safe to delete
  // factory before all authenticators it created are deleted.
  void SetAuthenticatorFactory(
      scoped_ptr<protocol::AuthenticatorFactory> authenticator_factory);

  // Sets the maximum duration of any session. By default, a session has no
  // maximum duration.
  void SetMaximumSessionDuration(const base::TimeDelta& max_session_duration);

  ////////////////////////////////////////////////////////////////////////////
  // ClientSession::EventHandler implementation.
  virtual void OnSessionAuthenticated(ClientSession* client) OVERRIDE;
  virtual void OnSessionChannelsConnected(ClientSession* client) OVERRIDE;
  virtual void OnSessionAuthenticationFailed(ClientSession* client) OVERRIDE;
  virtual void OnSessionClosed(ClientSession* session) OVERRIDE;
  virtual void OnSessionSequenceNumber(ClientSession* session,
                                       int64 sequence_number) OVERRIDE;
  virtual void OnSessionRouteChange(
      ClientSession* session,
      const std::string& channel_name,
      const protocol::TransportRoute& route) OVERRIDE;
  virtual void OnClientDimensionsChanged(ClientSession* session,
                                         const SkISize& size) OVERRIDE;

  // SessionManager::Listener implementation.
  virtual void OnSessionManagerReady() OVERRIDE;
  virtual void OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) OVERRIDE;

  // MouseMoveObserver interface.
  virtual void OnLocalMouseMoved(const SkIPoint& new_pos) OVERRIDE;

  // Sets desired configuration for the protocol. Must be called before Start().
  void set_protocol_config(scoped_ptr<protocol::CandidateSessionConfig> config);

  // Pause or unpause the session. While the session is paused, remote input
  // is ignored. Can be called from any thread.
  void PauseSession(bool pause);

  // Disconnects all active clients. Clients are disconnected
  // asynchronously when this method is called on a thread other than
  // the network thread. Potentically this may cause disconnection of
  // clients that were not connected when this method is called.
  void DisconnectAllClients();

 private:
  friend class base::RefCountedThreadSafe<ChromotingHost>;
  friend class ChromotingHostTest;

  typedef std::vector<scoped_refptr<ClientSession> > ClientList;

  enum State {
    kInitial,
    kStarted,
    kStopping,
    kStopped,
  };

  virtual ~ChromotingHost();

  // Called from Shutdown() to finish shutdown.
  void ShutdownFinish();

  // Unless specified otherwise all members of this class must be
  // used on the network thread only.

  // Parameters specified when the host was created.
  DesktopEnvironmentFactory* desktop_environment_factory_;
  scoped_ptr<protocol::SessionManager> session_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Connection objects.
  SignalStrategy* signal_strategy_;

  // Must be used on the network thread only.
  ObserverList<HostStatusObserver> status_observers_;

  // The connections to remote clients.
  ClientList clients_;

  // Tracks the internal state of the host.
  State state_;

  // Configuration of the protocol.
  scoped_ptr<protocol::CandidateSessionConfig> protocol_config_;

  // Login backoff state.
  net::BackoffEntry login_backoff_;

  // Flags used for RejectAuthenticatingClient().
  bool authenticating_client_;
  bool reject_authenticating_client_;

  // Stores list of tasks that should be executed when we finish
  // shutdown. Used only while |state_| is set to kStopping.
  std::vector<base::Closure> shutdown_tasks_;

  // The maximum duration of any session.
  base::TimeDelta max_session_duration_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_H_
