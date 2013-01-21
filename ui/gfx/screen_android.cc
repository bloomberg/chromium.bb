// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/display.h"

namespace gfx {

class ScreenAndroid : public Screen {
 public:
  ScreenAndroid() {}

  bool IsDIPEnabled() OVERRIDE {
    return true;
  }

  gfx::Point GetCursorScreenPoint() OVERRIDE {
    return gfx::Point();
  }

  gfx::NativeWindow GetWindowAtCursorScreenPoint() OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  gfx::Display GetPrimaryDisplay() const OVERRIDE {
    NOTIMPLEMENTED() << "crbug.com/117839 tracks implementation";
    return gfx::Display(0, gfx::Rect(0, 0, 1, 1));
  }

  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const OVERRIDE {
    return GetPrimaryDisplay();
  }

  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const OVERRIDE {
    return GetPrimaryDisplay();
  }

  int GetNumDisplays() OVERRIDE {
    return 1;
  }

  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE {
    return GetPrimaryDisplay();
  }

  virtual void AddObserver(DisplayObserver* observer) OVERRIDE {
    // no display change on Android.
  }

  virtual void RemoveObserver(DisplayObserver* observer) OVERRIDE {
    // no display change on Android.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAndroid);
};

Screen* CreateNativeScreen() {
  return new ScreenAndroid;
}

}  // namespace gfx
