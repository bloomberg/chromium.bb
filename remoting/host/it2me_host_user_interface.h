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
#include "base/memory/weak_ptr.h"

#include "remoting/host/host_user_interface.h"

namespace remoting {

class ContinueWindow;

// This class implements the IT2Me-specific parts of the ChromotingHost:
//   Disconnect and Continue window popups.
//   IT2Me-specific handling of multiple connection attempts.
class It2MeHostUserInterface : public HostUserInterface {
 public:
  It2MeHostUserInterface(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const UiStrings& ui_strings);
  virtual ~It2MeHostUserInterface();

  // HostUserInterface overrides.
  virtual void Init() OVERRIDE;

 protected:
  virtual void ProcessOnClientAuthenticated(
      const std::string& username) OVERRIDE;
  virtual void ProcessOnClientDisconnected() OVERRIDE;

  // Provide a user interface requiring the user to periodically re-confirm
  // the connection.
  scoped_ptr<ContinueWindow> continue_window_;

 private:
  class TimerTask;

  // Called by the ContinueWindow implementation (on the UI thread) when the
  // user dismisses the Continue prompt.
  void ContinueSession(bool continue_session);
  void OnContinueWindowTimer();
  void OnShutdownHostTimer();

  // Show or hide the Continue Sharing window on the UI thread.
  void ShowContinueWindow(bool show);
  void StartContinueWindowTimer(bool start);

  // Weak pointer factory used to abandon the "continue session" timer when
  // hiding the "continue session" dialog, or tearing down the IT2Me UI.
  base::WeakPtrFactory<It2MeHostUserInterface> timer_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHostUserInterface);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_HOST_USER_INTERFACE_H_
