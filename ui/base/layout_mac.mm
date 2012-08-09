// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <Cocoa/Cocoa.h>

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
@end

@interface NSWindow (LionAPI)
- (CGFloat)backingScaleFactor;
@end

namespace {

float GetScaleFactorScaleForNativeView(gfx::NativeView view) {
  float scale_factor = 1.0f;
  if (NSWindow* window = [view window]) {
    if ([window respondsToSelector:@selector(backingScaleFactor)])
      return [window backingScaleFactor];
    scale_factor = [window userSpaceScaleFactor];
  }
  if (NSScreen* screen = [NSScreen mainScreen]) {
    if ([screen respondsToSelector:@selector(backingScaleFactor)])
      return [screen backingScaleFactor];
    return [screen userSpaceScaleFactor];
  }
  return 1.0f;
}

}  // namespace

namespace ui {

ScaleFactor GetScaleFactorForNativeView(gfx::NativeView view) {
  return GetScaleFactorFromScale(GetScaleFactorScaleForNativeView(view));
}

}  // namespace ui
