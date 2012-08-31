// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#import <UIKit/UIKit.h>

#include "ui/gfx/display.h"

namespace gfx {

// static
gfx::Display Screen::GetPrimaryDisplay() {
  UIScreen* mainScreen = [[UIScreen screens] objectAtIndex:0];
  gfx::Display display(0, gfx::Rect(mainScreen.bounds));
  return display;
}

// static
int Screen::GetNumDisplays() {
#if TARGET_IPHONE_SIMULATOR
  // UIScreen does not reliably return correct results on the simulator.
  return 1;
#else
  return [[UIScreen screens] count];
#endif
}

}  // namespace gfx
