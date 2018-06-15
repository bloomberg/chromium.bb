// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_

#include <string>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

enum PlatformWindowType {
  PLATFORM_WINDOW_TYPE_WINDOW,
  PLATFORM_WINDOW_TYPE_POPUP,
  PLATFORM_WINDOW_TYPE_MENU,
};

// Initial properties which are passed to PlatformWindow to be initialized
// with a desired set of properties.
struct PlatformWindowInitProperties {
  // Tells desired PlatformWindow type. It can be popup, menu or anything else.
  PlatformWindowType type = PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW;
  // Sets the desired initial bounds. Can be empty.
  gfx::Rect bounds;
  // Tells PlatformWindow which native widget its parent holds. It is usually
  // used to find a parent from internal list of PlatformWindows.
  gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_INIT_PROPERTIES_H_
