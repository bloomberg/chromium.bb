// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_NATIVE_SCROLL_BAR_WRAPPER_H_
#define VIEWS_CONTROLS_NATIVE_SCROLL_BAR_WRAPPER_H_
#pragma once

namespace views {

class NativeScrollBar;
class View;

// A specialization of NativeControlWrapper that hosts a platform-native
// scroll bar.
class VIEWS_EXPORT NativeScrollBarWrapper {
 public:
  virtual ~NativeScrollBarWrapper() {}

  // Updates the scroll bar appearance given a viewport size, content size and
  // current position.
  virtual void Update(int viewport_size, int content_size, int current_pos) = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Returns the position of the scrollbar.
  virtual int GetPosition() const = 0;

  // Creates an appropriate NativeScrollBarWrapper for the platform.
  static NativeScrollBarWrapper* CreateWrapper(NativeScrollBar* button);

  // Returns the system sizes of vertical/horizontal scroll bars.
  static int GetVerticalScrollBarWidth();
  static int GetHorizontalScrollBarHeight();
};

}  // namespace views

#endif  // VIEWS_CONTROLS_NATIVE_SCROLL_BAR_WRAPPER_H_
