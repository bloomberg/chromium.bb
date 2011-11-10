// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_windows.h"

#include "ui/aura/window.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace test {

Window* CreateTestWindowWithId(int id, Window* parent) {
  return CreateTestWindowWithDelegate(NULL, id, gfx::Rect(), parent);
}

Window* CreateTestWindowWithBounds(const gfx::Rect& bounds, Window* parent) {
  return CreateTestWindowWithDelegate(NULL, 0, bounds, parent);
}

Window* CreateTestWindow(SkColor color,
                         int id,
                         const gfx::Rect& bounds,
                         Window* parent) {
  return CreateTestWindowWithDelegate(new ColorTestWindowDelegate(color),
                                      id, bounds, parent);
}

Window* CreateTestWindowWithDelegate(WindowDelegate* delegate,
                                     int id,
                                     const gfx::Rect& bounds,
                                     Window* parent) {
  Window* window = new Window(delegate);
  window->set_id(id);
  window->SetType(aura::WINDOW_TYPE_NORMAL);
  window->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window->SetBounds(bounds);
  window->Show();
  window->SetParent(parent);
  return window;
}

}  // namespace test
}  // namespace aura
