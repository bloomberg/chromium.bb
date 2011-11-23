// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONTINUE_WINDOW_H
#define REMOTING_HOST_CONTINUE_WINDOW_H

#include "base/callback.h"

namespace remoting {

class ChromotingHost;

class ContinueWindow {
 public:
  // ContinueSessionCallback is called when the user clicks on the
  // Continue button to resume the session, or dismisses the window to
  // terminate the session.  This callback is provided as a parameter to the
  // Show() method, and will be triggered on the UI thread.
  typedef base::Callback<void(bool)> ContinueSessionCallback;

  virtual ~ContinueWindow() {}

  // Show the continuation window requesting that the user approve continuing
  // the session.
  virtual void Show(ChromotingHost* host,
                    const ContinueSessionCallback& callback) = 0;

  // Hide the continuation window if it is visible.
  virtual void Hide() = 0;

  static ContinueWindow* Create();
};

}

#endif  // REMOTING_HOST_CONTINUE_WINDOW_H
