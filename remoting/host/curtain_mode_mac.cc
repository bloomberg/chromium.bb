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

CurtainMode::CurtainMode() : event_handler_(NULL) {
}

CurtainMode::~CurtainMode() {
  if (event_handler_) {
    RemoveEventHandler(event_handler_);
  }
}

bool CurtainMode::Init(const base::Closure& on_session_activate) {
  DCHECK(on_session_activate_.is_null());
  on_session_activate_ = on_session_activate;
  EventTypeSpec event;
  event.eventClass = kEventClassSystem;
  event.eventKind = kEventSystemUserSessionActivated;
  OSStatus result = InstallApplicationEventHandler(
      NewEventHandlerUPP(SessionActivateHandler), 1, &event, this,
      &event_handler_);
  return result == noErr;
}

void CurtainMode::OnClientAuthenticated(const std::string& jid) {
  // If the current session is attached to the console and is not showing
  // the logon screen then switch it out to ensure privacy.
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
    }
  }
}

OSStatus CurtainMode::SessionActivateHandler(EventHandlerCallRef handler,
                                             EventRef event,
                                             void* user_data) {
  CurtainMode* self = static_cast<CurtainMode*>(user_data);
  self->OnSessionActivate();
  return noErr;
}

void CurtainMode::OnSessionActivate() {
  if (!on_session_activate_.is_null()) {
    on_session_activate_.Run();
  }
}

}  // namespace remoting
