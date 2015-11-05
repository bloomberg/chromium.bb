// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_MANAGER_CLIENT_AREA_INSETS_H_
#define UI_VIEWS_MUS_WINDOW_MANAGER_CLIENT_AREA_INSETS_H_

#include "ui/gfx/geometry/insets.h"

namespace views {

struct WindowManagerClientAreaInsets {
  gfx::Insets normal_insets;
  gfx::Insets maximized_insets;
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_MANAGER_CLIENT_AREA_INSETS_H_
