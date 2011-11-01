// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain.h"

#include <ApplicationServices/ApplicationServices.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"

namespace {
static const char* kCGSessionPath =
    "/System/Library/CoreServices/Menu Extras/User.menu/Contents/Resources/"
    "CGSession";
}

namespace remoting {

namespace {

class CurtainMac : public Curtain {
 public:
  CurtainMac() {}
  virtual void EnableCurtainMode(bool enable) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CurtainMac);
};

void CurtainMac::EnableCurtainMode(bool enable) {
  // Whether curtain mode is being enabled or disabled, switch out the session
  // if the current user is switched in.
  // TODO(jamiewalch): If curtain mode is being enabled at the login screen, it
  // should be deferred until the user logs in.
  base::mac::ScopedCFTypeRef<CFDictionaryRef> session(
      CGSessionCopyCurrentDictionary());
  if (CFDictionaryGetValue(session,
                           kCGSessionOnConsoleKey) == kCFBooleanTrue) {
    pid_t child = fork();
    if (child == 0) {
      execl(kCGSessionPath, kCGSessionPath, "-suspend", (char*)0);
      exit(1);
    } else if (child > 0) {
      int status = 0;
      waitpid(child, &status, 0);
      // To ensure that the system has plenty of time to notify the CGDisplay-
      // ReconfigurationCallback, sleep here. 1s is probably overkill.
      sleep(1);
    }
  }
}

}  // namespace

Curtain* Curtain::Create() {
  // There's no need to check for curtain mode being enabled here because on
  // the mac it's easy for a local user to recover if anything crashes while
  // a session is active--they just have to enter a password to switch their
  // session back in.
  return new CurtainMac();
}

}  // namespace remoting
