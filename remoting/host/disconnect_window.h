// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DISCONNECT_WINDOW_H
#define REMOTING_HOST_DISCONNECT_WINDOW_H

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class ChromotingHost;

class DisconnectWindow {
 public:

  enum {
    kMaximumConnectedNameWidthInPixels = 400
  };

  // DisconnectCallback is called when the user clicks on the Disconnect button
  // to disconnect the session. This callback is provided as a parameter to the
  // Show() method, and will be triggered on the UI thread.
  typedef base::Callback<void(void)> DisconnectCallback;

  virtual ~DisconnectWindow() {}

  // Show the disconnect window allowing the user to shut down |host|.
  virtual void Show(ChromotingHost* host,
                    const DisconnectCallback& disconnect_callback,
                    const std::string& username) = 0;

  // Hide the disconnect window.
  virtual void Hide() = 0;

  static scoped_ptr<DisconnectWindow> Create();
};

}

#endif  // REMOTING_HOST_DISCONNECT_WINDOW_H
