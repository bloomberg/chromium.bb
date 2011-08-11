// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TYPES_H_
#define REMOTING_BASE_TYPES_H_

#include <set>

#include "ui/gfx/rect.h"

namespace remoting {

// The collection of 'invalid' screen rectangles.
// This is used to keep track of regions of the screen that require
// updating.
typedef std::set<gfx::Rect> InvalidRects;

}  // namespace remoting

#endif  // REMOTING_BASE_TYPES_H_
