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
  CurtainMode();
  virtual ~CurtainMode();

  // Set the callback to be invoked when the switched-out remote session is
  // switched back to the console.  Typically, remote connections should be
  // closed in this event to make sure that only one connection (console or
  // remote) exists to a session at any time to ensure privacy.  Note that
  // only the session's owner (or someone who knows the owner's password)
  // can attach it to the console, so this is safe.
  bool Init(const base::Closure& on_session_activate);

  // Overridden from HostStatusObserver
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;

 private:
  static OSStatus SessionActivateHandler(EventHandlerCallRef handler,
                                         EventRef event,
                                         void* user_data);
  void OnSessionActivate();

  base::Closure on_session_activate_;
  EventHandlerRef event_handler_;
};

}

#endif
