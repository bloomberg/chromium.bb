// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_DISPLAY_CONVERTER_H_
#define UI_VIEWS_MUS_DISPLAY_CONVERTER_H_

#include <vector>

#include "ui/gfx/display.h"
#include "ui/views/mus/mus_export.h"

namespace mus {
class Window;
}

namespace views {

std::vector<gfx::Display> VIEWS_MUS_EXPORT GetDisplaysFromWindow(
    mus::Window* window);

}  // namespace views

#endif  // UI_VIEWS_MUS_DISPLAY_CONVERTER_H_
