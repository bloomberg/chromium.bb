// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/rect.h"

namespace ui {

bool GrabViewSnapshot(gfx::NativeView view,
                      std::vector<unsigned char>* png_representation,
                      const gfx::Rect& snapshot_bounds) {
  png_representation->clear();

  base::ScopedCFTypeRef<CGImageRef> windowSnapshot(
      CGWindowListCreateImage(snapshot_bounds.ToCGRect(),
                              kCGWindowListOptionIncludingWindow,
                              [[view window] windowNumber],
                              kCGWindowImageBoundsIgnoreFraming));
  if (CGImageGetWidth(windowSnapshot) <= 0)
    return false;

  base::scoped_nsobject<NSBitmapImageRep> rep(
      [[NSBitmapImageRep alloc] initWithCGImage:windowSnapshot]);
  NSData* data = [rep representationUsingType:NSPNGFileType properties:nil];
  const unsigned char* buf = static_cast<const unsigned char*>([data bytes]);
  NSUInteger length = [data length];
  if (buf == NULL || length == 0)
    return false;

  png_representation->assign(buf, buf + length);
  DCHECK(!png_representation->empty());

  return true;
}

bool GrabWindowSnapshot(gfx::NativeWindow window,
                        std::vector<unsigned char>* png_representation,
                        const gfx::Rect& snapshot_bounds) {
  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  return GrabViewSnapshot([[window contentView] superview], png_representation,
      snapshot_bounds);
}

}  // namespace ui
