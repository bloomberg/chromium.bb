// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#import <Cocoa/Cocoa.h>

namespace gfx {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  NSPoint mouseLocation  = [NSEvent mouseLocation];
  // Flip coordinates to gfx (0,0 in top-left corner) using primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  mouseLocation.y = NSMaxY([screen frame]) - mouseLocation.y;
  return gfx::Point(mouseLocation.x, mouseLocation.y);
}

}  // namespace gfx
