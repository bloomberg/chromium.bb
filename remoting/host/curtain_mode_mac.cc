// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <Security/Security.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"

namespace {
const char* kCGSessionPath =
    "/System/Library/CoreServices/Menu Extras/User.menu/Contents/Resources/"
    "CGSession";
}

namespace remoting {

class CurtainModeMac : public CurtainMode {
 public:
  CurtainModeMac(const base::Closure& on_session_activate,
                 const base::Closure& on_error);

  virtual ~CurtainModeMac();

  // Overriden from CurtainMode.
  virtual void SetActivated(bool activated) OVERRIDE;

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
  EventHandlerRef event_handler_;

  DISALLOW_COPY_AND_ASSIGN(CurtainModeMac);
};

CurtainModeMac::CurtainModeMac(const base::Closure& on_session_activate,
                               const base::Closure& on_error)
    : on_session_activate_(on_session_activate),
      on_error_(on_error),
      event_handler_(NULL) {
}

CurtainModeMac::~CurtainModeMac() {
  SetActivated(false);
}

void CurtainModeMac::SetActivated(bool activated) {
  if (activated) {
    // Curtain-mode has issues on Mac OS X 10.7, so is currently disabled.
    //if (!ActivateCurtain()) {
    on_error_.Run();
    //}
  } else {
    RemoveEventHandler();
  }
}

bool CurtainModeMac::ActivateCurtain() {
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

OSStatus CurtainModeMac::SessionActivateHandler(EventHandlerCallRef handler,
                                             EventRef event,
                                             void* user_data) {
  CurtainModeMac* self = static_cast<CurtainModeMac*>(user_data);
  self->OnSessionActivate();
  return noErr;
}

void CurtainModeMac::OnSessionActivate() {
  on_session_activate_.Run();
}

bool CurtainModeMac::InstallEventHandler() {
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

bool CurtainModeMac::RemoveEventHandler() {
  OSStatus result = noErr;
  if (event_handler_) {
    result = ::RemoveEventHandler(event_handler_);
    event_handler_ = NULL;
  }
  return result == noErr;
}

// static
scoped_ptr<CurtainMode> CurtainMode::Create(
    const base::Closure& on_session_activate,
    const base::Closure& on_error) {
  return scoped_ptr<CurtainMode>(
      new CurtainModeMac(on_session_activate, on_error));
}

}  // namespace remoting
