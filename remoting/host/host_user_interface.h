// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_USER_INTERFACE_H_
#define REMOTING_HOST_HOST_USER_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

#include "remoting/base/scoped_thread_proxy.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class ChromotingHost;
class ChromotingHostContext;
class DisconnectWindow;
class LocalInputMonitor;
class SignalStrategy;

class HostUserInterface : public HostStatusObserver {
 public:
  HostUserInterface(ChromotingHostContext* context);
  virtual ~HostUserInterface();

  virtual void Start(ChromotingHost* host,
                     const base::Closure& disconnect_callback);

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 protected:
  const std::string& get_authenticated_jid() const {
    return authenticated_jid_;
  }
  ChromotingHost* get_host() const { return host_; }

  // Message loop accessors.
  base::MessageLoopProxy* network_message_loop() const;
  base::MessageLoopProxy* ui_message_loop() const;

  // Invokes the session disconnect callback passed to Start().
  void DisconnectSession() const;

  virtual void ProcessOnClientAuthenticated(const std::string& username);
  virtual void ProcessOnClientDisconnected();

  // Used by unit-tests as an alternative to Start() so that mock versions of
  // internal objects can be used.
  void StartForTest(
      ChromotingHost* host,
      const base::Closure& disconnect_callback,
      scoped_ptr<DisconnectWindow> disconnect_window,
      scoped_ptr<LocalInputMonitor> local_input_monitor);

 private:
  // Invoked from the UI thread when the user clicks on the Disconnect button
  // to disconnect the session.
  void OnDisconnectCallback();

  void MonitorLocalInputs(bool enable);

  // Show or hide the Disconnect window on the UI thread.  If |show| is false,
  // hide the window, ignoring the |username| parameter.
  void ShowDisconnectWindow(bool show, const std::string& username);

  // The JID of the currently-authenticated user (or an empty string if no user
  // is connected).
  std::string authenticated_jid_;

  ChromotingHost* host_;

  // Host context used to make sure operations are run on the correct thread.
  // This is owned by the ChromotingHost.
  ChromotingHostContext* context_;

  // Used to ask the host to disconnect the session.
  base::Closure disconnect_callback_;

  // Provide a user interface allowing the host user to close the connection.
  scoped_ptr<DisconnectWindow> disconnect_window_;

  // Monitor local inputs to allow remote inputs to be blocked while the local
  // user is trying to do something.
  scoped_ptr<LocalInputMonitor> local_input_monitor_;

  bool is_monitoring_local_inputs_;

  ScopedThreadProxy ui_thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(HostUserInterface);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_USER_INTERFACE_H_
