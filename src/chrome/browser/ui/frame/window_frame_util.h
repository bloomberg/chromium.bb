// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FRAME_WINDOW_FRAME_UTIL_H_
#define CHROME_BROWSER_UI_FRAME_WINDOW_FRAME_UTIL_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Size;
}

// Static-only class containing values and helper functions for frame classes
// that need to be accessible outside of /browser/ui/views.
class WindowFrameUtil {
 public:
  static constexpr int kWindows10GlassCaptionButtonWidth = 45;
  static constexpr int kWindows10GlassCaptionButtonHeightRestored = 29;
  static constexpr int kWindows10GlassCaptionButtonVisualSpacing = 1;

  // Returns the alpha that the Windows10CaptionButton should use to blend the
  // color provided by the theme in determining the button's 'base color'.
  static SkAlpha CalculateWindows10GlassCaptionButtonBackgroundAlpha(
      SkAlpha theme_alpha);

  // Returns the size of the area occupied by the caption buttons in the glass
  // browser frame view.
  static gfx::Size GetWindows10GlassCaptionButtonAreaSize();

 private:
  WindowFrameUtil() {}

  DISALLOW_COPY_AND_ASSIGN(WindowFrameUtil);
};

#endif  // CHROME_BROWSER_UI_FRAME_WINDOW_FRAME_UTIL_H_
