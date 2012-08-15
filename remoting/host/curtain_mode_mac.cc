// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Security/Security.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"

namespace {
const char* kCGSessionPath =
    "/System/Library/CoreServices/Menu Extras/User.menu/Contents/Resources/"
    "CGSession";
}

namespace remoting {

CurtainMode::CurtainMode(const base::Closure& on_session_activate,
                         const base::Closure& on_error)
    : on_session_activate_(on_session_activate),
      on_error_(on_error),
      connection_active_(false),
      event_handler_(NULL) {
}

CurtainMode::~CurtainMode() {
  SetEnabled(false);
}

void CurtainMode::SetEnabled(bool enabled) {
  if (enabled) {
    if (connection_active_) {
      if (!ActivateCurtain()) {
        on_error_.Run();
      }
    }
  } else {
    RemoveEventHandler();
  }
}

bool CurtainMode::ActivateCurtain() {
  // Try to install the switch-in handler. Do this before switching out the
  // current session so that the console session is not affected if it fails.
  if (!InstallEventHandler()) {
    return false;
  }

  base::mac::ScopedCFTypeRef<CFDictionaryRef> session(
      CGSessionCopyCurrentDictionary());
  const void* on_console = CFDictionaryGetValue(session,
                                                kCGSessionOnConsoleKey);
  const void* logged_in = CFDictionaryGetValue(session, kCGSessionLoginDoneKey);
  if (logged_in == kCFBooleanTrue && on_console == kCFBooleanTrue) {
    pid_t child = fork();
    if (child == 0) {
      execl(kCGSessionPath, kCGSessionPath, "-suspend", NULL);
      _exit(1);
    } else if (child > 0) {
      int status = 0;
      waitpid(child, &status, 0);
      if (status != 0) {
        LOG(ERROR) << kCGSessionPath << " failed.";
        return false;
      }
    } else {
      LOG(ERROR) << "fork() failed.";
      return false;
    }
  }
  return true;
}

// TODO(jamiewalch): This code assumes at most one client connection at a time.
// Add OnFirstClientConnected and OnLastClientDisconnected optional callbacks
// to the HostStatusObserver interface to address this.
void CurtainMode::OnClientAuthenticated(const std::string& jid) {
  connection_active_ = true;
  SetEnabled(true);
}

void CurtainMode::OnClientDisconnected(const std::string& jid) {
  SetEnabled(false);
  connection_active_ = false;
}

OSStatus CurtainMode::SessionActivateHandler(EventHandlerCallRef handler,
                                             EventRef event,
                                             void* user_data) {
  CurtainMode* self = static_cast<CurtainMode*>(user_data);
  self->OnSessionActivate();
  return noErr;
}

void CurtainMode::OnSessionActivate() {
  on_session_activate_.Run();
}

bool CurtainMode::InstallEventHandler() {
  OSStatus result = noErr;
  if (!event_handler_) {
    EventTypeSpec event;
    event.eventClass = kEventClassSystem;
    event.eventKind = kEventSystemUserSessionActivated;
    result = ::InstallApplicationEventHandler(
        NewEventHandlerUPP(SessionActivateHandler), 1, &event, this,
        &event_handler_);
  }
  return result == noErr;
}

bool CurtainMode::RemoveEventHandler() {
  OSStatus result = noErr;
  if (event_handler_) {
    result = ::RemoveEventHandler(event_handler_);
  }
  return result == noErr;
}

}  // namespace remoting
