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

  virtual ~DisconnectWindow() {}

  // Shows the disconnect window, allowing the user to disconnect the session.
  // |disconnect_callback| will be invoked on the calling UI thread when the
  // user chooses to disconnect, or if the window is closed by any means other
  // than Hide(), or deletion of the DisconnectWindow instance.
  // Show returns false if the window cannot be shown, in which case the
  // callback will not be invoked.
  virtual bool Show(const base::Closure& disconnect_callback,
                    const std::string& username) = 0;

  // Hides the disconnect window. The disconnect callback will not be invoked.
  virtual void Hide() = 0;

  // |ui_strings| specifies localized strings to be used by the window.
  // |ui_strings| must outlive the returned object.
  static scoped_ptr<DisconnectWindow> Create(const UiStrings* ui_strings);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DISCONNECT_WINDOW_H_
