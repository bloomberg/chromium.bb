// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_MODE_H_
#define REMOTING_HOST_CURTAIN_MODE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"

namespace remoting {

class ChromotingHost;

class CurtainMode {
 public:
  virtual ~CurtainMode() {}

  // Creates a CurtainMode object, with callbacks to be invoked when the
  // switched-out session is switched back to the console, or in case of errors
  // activating the curtain. Typically, remote clients should be disconnected in
  // both cases: for errors, because the privacy guarantee of curtain-mode
  // cannot be honoured; for switch-in, to ensure that only one connection
  // (console or remote) exists to a session.
  static scoped_ptr<CurtainMode> Create(
      const base::Closure& on_session_activate,
      const base::Closure& on_error);

  // Activate or deactivate curtain mode.
  // If activated is true (meaning a remote client has just logged in), the
  // implementation must immediately activate the curtain, or call on_error if
  // it cannot do so. If a console user logs in while the curtain is activated,
  // the implementation must call on_session_activate (from any thread).
  // If activated is false (meaning the remote client has disconnected), the
  // implementation must not remove the curtain (since at this point we can make
  // no guarantees about whether the user intended to leave the console locked),
  // and must not call on_session_activate when the console user logs in.
  virtual void SetActivated(bool activated) = 0;

 protected:
  CurtainMode() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CurtainMode);
};

}  // namespace remoting

#endif
