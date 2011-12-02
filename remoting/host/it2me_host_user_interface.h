// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_
#define REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

#include "remoting/base/scoped_thread_proxy.h"
#include "remoting/host/host_status_observer.h"

// Milliseconds before the continue window is shown.
static const int kContinueWindowShowTimeoutMs = 10 * 60 * 1000;

// Milliseconds before the continue window is automatically dismissed and
// the connection is closed.
static const int kContinueWindowHideTimeoutMs = 60 * 1000;

namespace remoting {

class ChromotingHost;
class ChromotingHostContext;
class ContinueWindow;
class DisconnectWindow;
class LocalInputMonitor;
class SignalStrategy;

// This class implements the IT2Me-specific parts of the ChromotingHost:
//   Disconnect and Continue window popups.
//   IT2Me-specific handling of multiple connection attempts.
class It2MeHostUserInterface : public HostStatusObserver {
 public:
  It2MeHostUserInterface(ChromotingHost* host, ChromotingHostContext* context);
  virtual ~It2MeHostUserInterface();

  void Init();

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnSignallingConnected(SignalStrategy* signal_strategy,
                                     const std::string& full_jid) OVERRIDE;
  virtual void OnSignallingDisconnected() OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied() OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // Shuts down the object and all its resources synchronously. Must
  // be called on the UI thread.
  void Shutdown();

 private:
  class TimerTask;

  // Allow ChromotingHostTest::SetUp() to call InitFrom().
  friend class ChromotingHostTest;

  // Used by unit-tests as an alternative to Init() so that mock versions of
  // internal objects can be used.  This takes ownership of all objects passed
  // in.
  void InitFrom(DisconnectWindow* disconnect_window,
                ContinueWindow* continue_window,
                LocalInputMonitor* monitor);

  void ProcessOnClientAuthenticated(const std::string& username);
  void ProcessOnClientDisconnected();

  void MonitorLocalInputs(bool enable);

  // Show or hide the Disconnect window on the UI thread.  If |show| is false,
  // hide the window, ignoring the |username| parameter.
  void ShowDisconnectWindow(bool show, const std::string& username);

  // Show or hide the Continue Sharing window on the UI thread.
  void ShowContinueWindow(bool show);

  // Called by the ContinueWindow implementation (on the UI thread) when the
  // user dismisses the Continue prompt.
  void ContinueSession(bool continue_session);

  void StartContinueWindowTimer(bool start);

  void OnContinueWindowTimer();
  void OnShutdownHostTimer();

  ChromotingHost* host_;

  // Host context used to make sure operations are run on the correct thread.
  // This is owned by the ChromotingHost.
  ChromotingHostContext* context_;

  // Provide a user interface allowing the host user to close the connection.
  scoped_ptr<DisconnectWindow> disconnect_window_;

  // Provide a user interface requiring the user to periodically re-confirm
  // the connection.
  scoped_ptr<ContinueWindow> continue_window_;

  // Monitor local inputs to allow remote inputs to be blocked while the local
  // user is trying to do something.
  scoped_ptr<LocalInputMonitor> local_input_monitor_;

  bool is_monitoring_local_inputs_;

  // Timer controlling the "continue session" dialog.
  scoped_ptr<TimerTask> timer_task_;

  ScopedThreadProxy ui_thread_proxy_;

  // The JID of the currently-authenticated user (or an empty string if no user
  // is connected).
  std::string authenticated_jid_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHostUserInterface);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_
