// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux_util.h"

#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdlib.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/linux_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/gfx/rect.h"

namespace {

// Common implementation of ConvertAcceleratorsFromWindowsStyle() and
// RemoveWindowsStyleAccelerators().
// Replaces all ampersands (as used in our grd files to indicate mnemonics)
// to |target|. Similarly any underscores get replaced with two underscores as
// is needed by GTK.
std::string ConvertAmperstandsTo(const std::string& label,
                                 const std::string& target) {
  std::string ret;
  ret.reserve(label.length() * 2);
  for (size_t i = 0; i < label.length(); ++i) {
    if ('_' == label[i]) {
      ret.push_back('_');
      ret.push_back('_');
    } else if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back('&');
        ++i;
      } else {
        ret.append(target);
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

}  // namespace

namespace gfx {

#if defined(USE_WAYLAND) || defined(USE_AURA)
PangoContext* GetPangoContext() {
  PangoFontMap* font_map = pango_cairo_font_map_get_default();
  return pango_font_map_create_context(font_map);
}
#endif

double GetPangoResolution() {
  static double resolution;
  static bool determined_resolution = false;
  if (!determined_resolution) {
    determined_resolution = true;
    PangoContext* default_context = GetPangoContext();
    resolution = pango_cairo_context_get_resolution(default_context);
    g_object_unref(default_context);
  }
  return resolution;
}

std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label) {
  return ConvertAmperstandsTo(label, "_");
}

std::string RemoveWindowsStyleAccelerators(const std::string& label) {
  return ConvertAmperstandsTo(label, "");
}

uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride) {
  if (stride == 0)
    stride = width * 4;

  uint8_t* new_pixels = static_cast<uint8_t*>(malloc(height * stride));

  // We have to copy the pixels and swap from BGRA to RGBA.
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int idx = i * stride + j * 4;
      new_pixels[idx] = pixels[idx + 2];
      new_pixels[idx + 1] = pixels[idx + 1];
      new_pixels[idx + 2] = pixels[idx];
      new_pixels[idx + 3] = pixels[idx + 3];
    }
  }

  return new_pixels;
}

}  // namespace gfx
