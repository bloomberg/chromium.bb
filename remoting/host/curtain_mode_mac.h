// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_MODE_MAC_H_
#define REMOTING_HOST_CURTAIN_MODE_MAC_H_

#include <Carbon/Carbon.h>

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

class CurtainMode : public HostStatusObserver {
 public:
  // Set callbacks to be invoked when the switched-out session is switched back
  // to the console, or in case of errors activating curtain-mode.  Typically,
  // remote clients should be disconnected in both cases: for errors, because
  // the privacy guarantee of curtain-mode cannot be honoured; for switch-in,
  // to ensure that only one connection (console or remote) exists to a session.
  // Note that only the session's owner (or someone who knows the password) can
  // attach it to the console, so this is safe.
  CurtainMode(const base::Closure& on_session_activate,
              const base::Closure& on_error);
  virtual ~CurtainMode();

  // Enable or disable curtain-mode. Note that disabling curtain-mode does not
  // deactivate the curtain, but it does remove the switch-in handler, meaning
  // that on_session_activate will not be invoked if curtain-mode is disabled.
  // Conversely, enabling curtain-mode *does* activate the curtain if there is
  // a connected client at the time.
  void SetEnabled(bool enabled);

  // Overridden from HostStatusObserver
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;

 private:
  // If the current session is attached to the console and is not showing
  // the logon screen then switch it out to ensure privacy.
  bool ActivateCurtain();

  // Add or remove the switch-in event handler.
  bool InstallEventHandler();
  bool RemoveEventHandler();

  // Handlers for the switch-in event.
  static OSStatus SessionActivateHandler(EventHandlerCallRef handler,
                                         EventRef event,
                                         void* user_data);
  void OnSessionActivate();

  base::Closure on_session_activate_;
  base::Closure on_error_;
  bool connection_active_;
  EventHandlerRef event_handler_;
};

}

#endif
