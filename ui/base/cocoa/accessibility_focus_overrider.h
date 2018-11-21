// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_ACCESSIBILITY_FOCUS_OVERRIDER_H_
#define UI_BASE_COCOA_ACCESSIBILITY_FOCUS_OVERRIDER_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// This object will swizzle -[NSApplication accessibilityFocusedUIElement] to
// return the value of its client's GetAccessibilityFocusedUIElement method
// whenever both |window_is_key_| is true and |view_is_first_responder_| is
// true. If two instances both claim that their window is key and their view
// is first responder at the same time, then the more recent instance to have
// made that claim will "win". This mechanism is used for accessibility in
// RemoteMacViews.
class UI_BASE_EXPORT AccessibilityFocusOverrider {
 public:
  class Client {
   public:
    virtual id GetAccessibilityFocusedUIElement() = 0;
  };

  AccessibilityFocusOverrider(Client* client);
  ~AccessibilityFocusOverrider();

  // Indicate whether or not the view's window is currently key. This object
  // will override the application's focused accessibility element only if its
  // window is key (and the view is the window's first responder).
  void SetWindowIsKey(bool window_is_key);

  // Indicate whether or not the view is its window's first responder. This
  // object will override the application's focused accessibility element only
  // if the view is the window's first responder (and its window is key).
  void SetViewIsFirstResponder(bool view_is_first_responder);

 private:
  void UpdateOverriddenKeyElement();
  bool window_is_key_ = false;
  bool view_is_first_responder_ = false;
  Client* const client_;
};

}  // namespace ui

#endif  // UI_BASE_COCOA_ACCESSIBILITY_FOCUS_OVERRIDER_H_
