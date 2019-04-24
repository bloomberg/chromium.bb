// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_SKIA_UTILS_GTK_H_
#define CHROME_BROWSER_UI_LIBGTKUI_SKIA_UTILS_GTK_H_

#include <gtk/gtk.h>
#include <stdint.h>

#include "third_party/skia/include/core/SkColor.h"

#if !GTK_CHECK_VERSION(3, 90, 0)
typedef struct _GdkColor GdkColor;
#endif
typedef struct _GdkPixbuf GdkPixbuf;

class SkBitmap;

#if !GTK_CHECK_VERSION(3, 90, 0)
// Define a macro for creating GdkColors from RGB values.  This is a macro to
// allow static construction of literals, etc.  Use this like:
//   GdkColor white = GDK_COLOR_RGB(0xff, 0xff, 0xff);
#define GDK_COLOR_RGB(r, g, b)               \
  {                                          \
    0,                                       \
    r * ::libgtkui::kSkiaToGDKMultiplier,    \
    g * ::libgtkui::kSkiaToGDKMultiplier,    \
    b * ::libgtkui::kSkiaToGDKMultiplier,    \
  }
#endif

namespace libgtkui {

#if !GTK_CHECK_VERSION(3, 90, 0)
// Multiply uint8_t color components by this.
const int kSkiaToGDKMultiplier = 257;

// Converts GdkColors to the ARGB layout Skia expects.
SkColor GdkColorToSkColor(GdkColor color);

// Converts ARGB to GdkColor.
GdkColor SkColorToGdkColor(SkColor color);
#endif

const SkBitmap GdkPixbufToImageSkia(GdkPixbuf* pixbuf);

// Convert and copy a SkBitmap to a GdkPixbuf. NOTE: this uses BGRAToRGBA, so
// it is an expensive operation.  The returned GdkPixbuf will have a refcount of
// 1, and the caller is responsible for unrefing it when done.
GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap& bitmap);

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_SKIA_UTILS_GTK_H_
