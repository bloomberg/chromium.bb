// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DISCONNECT_WINDOW_H_
#define REMOTING_HOST_DISCONNECT_WINDOW_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

struct UiStrings;

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

  // Shows the disconnect window allowing the user to disconnect the session.
  // Returns false if the window could not be shown for any reason.
  virtual bool Show(const UiStrings& ui_strings,
                    const DisconnectCallback& disconnect_callback,
                    const std::string& username) = 0;

  // Hides the disconnect window.
  virtual void Hide() = 0;

  static scoped_ptr<DisconnectWindow> Create();
};

}  // namespace remoting

#endif  // REMOTING_HOST_DISCONNECT_WINDOW_H_
