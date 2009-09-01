// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_gtk.h"

#include <gdk/gdkcolor.h>

namespace skia {

const int kSkiaToGDKMultiplier = 257;

SkColor GdkColorToSkColor(GdkColor color) {
  return SkColorSetRGB(color.red, color.green, color.blue);
}

GdkColor SkColorToGdkColor(SkColor color) {
  GdkColor gdk_color = {
      0,
      SkColorGetR(color) * kSkiaToGDKMultiplier,
      SkColorGetG(color) * kSkiaToGDKMultiplier,
      SkColorGetB(color) * kSkiaToGDKMultiplier
  };
  return gdk_color;
}

}  // namespace
