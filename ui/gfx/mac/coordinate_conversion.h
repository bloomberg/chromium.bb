// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_COORDINATE_CONVERSION_H_
#define UI_GFX_MAC_COORDINATE_CONVERSION_H_

#import <Foundation/Foundation.h>

#include "ui/gfx/gfx_export.h"

namespace gfx {

class Rect;

// Convert a gfx::Rect specified with the origin at the top left of the primary
// display into AppKit secreen coordinates (origin at the bottom left).
GFX_EXPORT NSRect ScreenRectToNSRect(const gfx::Rect& rect);

// Convert an AppKit NSRect with origin in the bottom left of the primary
// display into a gfx::Rect with origin at the top left of the primary display.
GFX_EXPORT gfx::Rect ScreenRectFromNSRect(const NSRect& point);

}  // namespace gfx

#endif  // UI_GFX_MAC_COORDINATE_CONVERSION_H_
