// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_
#define REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

#include "remoting/host/host_user_interface.h"

namespace remoting {

class ContinueWindow;

// This class implements the IT2Me-specific parts of the ChromotingHost:
//   Disconnect and Continue window popups.
//   IT2Me-specific handling of multiple connection attempts.
class It2MeHostUserInterface : public HostUserInterface {
 public:
  It2MeHostUserInterface(ChromotingHostContext* context);
  virtual ~It2MeHostUserInterface();

  virtual void Start(ChromotingHost* host,
                     const base::Closure& disconnect_callback)  OVERRIDE;

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;

 protected:
  virtual void ProcessOnClientAuthenticated(
      const std::string& username) OVERRIDE;
  virtual void ProcessOnClientDisconnected() OVERRIDE;

 private:
  class TimerTask;

  // Allow ChromotingHostTest::SetUp() to call StartForTest().
  friend class ChromotingHostTest;

  // Used by unit-tests as an alternative to Start() so that mock versions of
  // internal objects can be used.
  void StartForTest(
      ChromotingHost* host,
      const base::Closure& disconnect_callback,
      scoped_ptr<DisconnectWindow> disconnect_window,
      scoped_ptr<ContinueWindow> continue_window,
      scoped_ptr<LocalInputMonitor> local_input_monitor);

  // Called by the ContinueWindow implementation (on the UI thread) when the
  // user dismisses the Continue prompt.
  void ContinueSession(bool continue_session);
  void OnContinueWindowTimer();
  void OnShutdownHostTimer();

  // Show or hide the Continue Sharing window on the UI thread.
  void ShowContinueWindow(bool show);
  void StartContinueWindowTimer(bool start);

  // Provide a user interface requiring the user to periodically re-confirm
  // the connection.
  scoped_ptr<ContinueWindow> continue_window_;

  // Timer controlling the "continue session" dialog.
  scoped_ptr<TimerTask> timer_task_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHostUserInterface);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_
