// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_H_
#define REMOTING_HOST_CHROMOTING_HOST_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "remoting/base/encoder.h"
#include "remoting/host/capturer.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/ui_strings.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/connection_to_client.h"

namespace remoting {

namespace protocol {
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
                       public ClientSession::EventHandler,
                       public SignalStrategy::StatusObserver,
                       public protocol::SessionManager::Listener {
 public:
  // Factory methods that must be used to create ChromotingHost
  // instances. It does NOT take ownership of |context|, and
  // |environment|, but they should not be deleted until returned host
  // is destroyed.
  static ChromotingHost* Create(ChromotingHostContext* context,
                                MutableHostConfig* config,
                                DesktopEnvironment* environment,
                                bool allow_nat_traversal);

  // Asynchronously start the host process.
  //
  // After this is invoked, the host process will connect to the talk
  // network and start listening for incoming connections.
  //
  // This method can only be called once during the lifetime of this object.
  void Start();

  // Asynchronously shutdown the host process. |shutdown_task| is
  // called after shutdown is completed.
  void Shutdown(const base::Closure& shutdown_task);

  // Adds |observer| to the list of status observers. Doesn't take
  // ownership of |observer|, so |observer| must outlive this
  // object. All status observers must be added before the host is
  // started.
  void AddStatusObserver(HostStatusObserver* observer);

  // Sets shared secret for the host. All incoming connections are
  // rejected if shared secret isn't set. Must be called on the
  // network thread after the host is started.
  void SetSharedSecret(const std::string& shared_secret);

  ////////////////////////////////////////////////////////////////////////////
  // SignalStrategy::StatusObserver implementation.
  virtual void OnStateChange(
      SignalStrategy::StatusObserver::State state) OVERRIDE;
  virtual void OnJidChange(const std::string& full_jid) OVERRIDE;

  ////////////////////////////////////////////////////////////////////////////
  // ClientSession::EventHandler implementation.
  virtual void OnSessionAuthenticated(ClientSession* client) OVERRIDE;
  virtual void OnSessionAuthenticationFailed(ClientSession* client) OVERRIDE;
  virtual void OnSessionClosed(ClientSession* session) OVERRIDE;
  virtual void OnSessionSequenceNumber(ClientSession* session,
                                       int64 sequence_number) OVERRIDE;

  // SessionManager::Listener implementation.
  virtual void OnSessionManagerInitialized() OVERRIDE;
  virtual void OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) OVERRIDE;

  // Sets desired configuration for the protocol. Ownership of the
  // |config| is transferred to the object. Must be called before Start().
  void set_protocol_config(protocol::CandidateSessionConfig* config);

  // TODO(wez): ChromotingHost shouldn't need to know about Me2Mom.
  void set_it2me(bool is_it2me) {
    is_it2me_ = is_it2me;
  }

  // Notify all active client sessions that local input has been detected, and
  // that remote input should be ignored for a short time.
  void LocalMouseMoved(const SkIPoint& new_pos);

  // Pause or unpause the session. While the session is paused, remote input
  // is ignored.
  void PauseSession(bool pause);

  const UiStrings& ui_strings() { return ui_strings_; }

  // Set localized strings. Must be called before host is started.
  void SetUiStrings(const UiStrings& ui_strings);

 private:
  friend class base::RefCountedThreadSafe<ChromotingHost>;
  friend class ChromotingHostTest;

  typedef std::vector<HostStatusObserver*> StatusObserverList;
  typedef std::vector<ClientSession*> ClientList;

  enum State {
    kInitial,
    kStarted,
    kStopping,
    kStopped,
  };

  // Caller keeps ownership of |context| and |environment|.
  ChromotingHost(ChromotingHostContext* context,
                 MutableHostConfig* config,
                 DesktopEnvironment* environment,
                 bool allow_nat_traversal);
  virtual ~ChromotingHost();

  // Creates encoder for the specified configuration.
  Encoder* CreateEncoder(const protocol::SessionConfig& config);

  std::string GenerateHostAuthToken(const std::string& encoded_client_token);

  void AddAuthenticatedClient(ClientSession* client,
                              const protocol::SessionConfig& config,
                              const std::string& jid);

  int AuthenticatedClientsCount() const;

  void StopScreenRecorder();
  void OnScreenRecorderStopped();

  // Called from Shutdown() or OnScreenRecorderStopped() to finish shutdown.
  void ShutdownFinish();

  // Unless specified otherwise all members of this class must be
  // used on the network thread only.

  // Parameters specified when the host was created.
  ChromotingHostContext* context_;
  DesktopEnvironment* desktop_environment_;
  scoped_refptr<MutableHostConfig> config_;
  HostKeyPair key_pair_;
  bool allow_nat_traversal_;

  // TODO(lambroslambrou): The following is a temporary fix for Me2Me
  // (crbug.com/105995), pending the AuthenticatorFactory work.
  // Cache the shared secret, in case SetSharedSecret() is called before the
  // session manager has been created.
  // The |have_shared_secret_| flag is to distinguish SetSharedSecret() not
  // being called at all, from being called with an empty string.
  std::string shared_secret_;
  bool have_shared_secret_;

  // Connection objects.
  scoped_ptr<SignalStrategy> signal_strategy_;
  std::string local_jid_;
  scoped_ptr<protocol::SessionManager> session_manager_;

  // StatusObserverList is thread-safe and can be used on any thread.
  StatusObserverList status_observers_;

  // The connections to remote clients.
  ClientList clients_;

  // Session manager for the host process.
  scoped_refptr<ScreenRecorder> recorder_;

  // Number of screen recorders that are currently being
  // stopped. Normally set to 0 or 1, but in some cases it may be
  // greater than 1, particularly if when second client can connect
  // immediately after previous one disconnected.
  int stopping_recorders_;

  // Tracks the internal state of the host.
  State state_;

  // Configuration of the protocol.
  scoped_ptr<protocol::CandidateSessionConfig> protocol_config_;

  // Stores list of tasks that should be executed when we finish
  // shutdown. Used only while |state_| is set to kStopping.
  std::vector<base::Closure> shutdown_tasks_;

  // TODO(sergeyu): The following members do not belong to
  // ChromotingHost and should be moved elsewhere.
  bool is_it2me_;
  std::string access_code_;
  UiStrings ui_strings_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_H_
