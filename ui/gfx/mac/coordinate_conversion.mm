// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/mac/coordinate_conversion.h"

#import <Cocoa/Cocoa.h>

#include "ui/gfx/geometry/rect.h"

namespace gfx {

namespace {

// The height of the primary display, which OSX defines as the monitor with the
// menubar. This is always at index 0.
CGFloat PrimaryDisplayHeight() {
  return NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
}

}  // namespace

NSRect ScreenRectToNSRect(const gfx::Rect& rect) {
  return NSMakeRect(rect.x(),
                    PrimaryDisplayHeight() - rect.y() - rect.height(),
                    rect.width(),
                    rect.height());
}

gfx::Rect ScreenRectFromNSRect(const NSRect& rect) {
  return gfx::Rect(rect.origin.x,
                   PrimaryDisplayHeight() - rect.origin.y - rect.size.height,
                   rect.size.width,
                   rect.size.height);
}

}  // namespace gfx
