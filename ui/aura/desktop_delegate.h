// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DELEGATE_H_
#define UI_AURA_DESKTOP_DELEGATE_H_
#pragma once

namespace aura {

class Window;

class DesktopDelegate {
 public:
  virtual ~DesktopDelegate() {}

  // Called by the Window when its parent is set to NULL. The delegate is given
  // an opportunity to inspect the window and add it to a default parent window
  // of its choosing.
  virtual void AddChildToDefaultParent(Window* window) = 0;

  // Returns the window that should be activated other than |ignore|.
  virtual Window* GetTopmostWindowToActivate(Window* ignore) const = 0;
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DELEGATE_H_
