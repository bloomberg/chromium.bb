// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_OBSERVER_H_
#define UI_AURA_ROOT_WINDOW_OBSERVER_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace gfx {
class Size;
}

namespace aura {
class RootWindow;
class Window;

class AURA_EXPORT RootWindowObserver {
 public:
  // Invoked after the RootWindow is resized.
  virtual void OnRootWindowResized(const RootWindow* root,
                                   const gfx::Size& old_size) {}

  // Invoked when the native windowing system sends us a request to close our
  // window.
  virtual void OnRootWindowHostClosed(const RootWindow* root) {}

  // Invoked when the keyboard mapping has changed.
  virtual void OnKeyboardMappingChanged(const RootWindow* root) {}

 protected:
  virtual ~RootWindowObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_OBSERVER_H_
