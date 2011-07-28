// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_SHAPE_H_
#define VIEWS_WINDOW_WINDOW_SHAPE_H_
#pragma once

#include "views/views_api.h"

namespace gfx {
class Size;
class Path;
}

namespace views {

// Sets the window mask to a style that most likely matches
// ui/resources/window_*
VIEWS_API void GetDefaultWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask);

} // namespace views

#endif  // VIEWS_WINDOW_WINDOW_SHAPE_H_
