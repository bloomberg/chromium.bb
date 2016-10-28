// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_

#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}

namespace aura {

class Window;

class AURA_EXPORT WindowTreeHostMusDelegate {
 public:
  // Called to set the bounds of the root window. |bounds| is the bounds
  // supplied to SetBounds() and may be adjusted by the default. After this call
  // |bounds| is applied to WindowTreeHost. |bounds| is in dips.
  virtual void SetRootWindowBounds(Window* window, gfx::Rect* bounds) = 0;

 protected:
  virtual ~WindowTreeHostMusDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_
