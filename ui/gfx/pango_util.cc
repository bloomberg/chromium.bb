// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/pango_util.h"

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <string>

#include <algorithm>
#include <map>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/gfx/platform_font_pango.h"
#include "ui/gfx/text_utils.h"

namespace gfx {

namespace {

// Creates and returns a PangoContext. The caller owns the context.
PangoContext* GetPangoContext() {
  PangoFontMap* font_map = pango_cairo_font_map_get_default();
  return pango_font_map_create_context(font_map);
}

// Returns the DPI that should be used by Pango.
double GetPangoDPI() {
  static double dpi = -1.0;
  if (dpi < 0.0) {
    const gfx::LinuxFontDelegate* delegate = gfx::LinuxFontDelegate::instance();
    if (delegate)
      dpi = delegate->GetFontDPI();
    if (dpi <= 0.0)
      dpi = 96.0;
  }
  return dpi;
}

// Returns the number of pixels in a point.
// - multiply a point size by this to get pixels ("device units")
// - divide a pixel size by this to get points
double GetPixelsInPoint() {
  static double pixels_in_point = GetPangoDPI() / 72.0;  // 72 points in an inch
  return pixels_in_point;
}

}  // namespace

int GetPangoFontSizeInPixels(PangoFontDescription* pango_font) {
  // If the size is absolute, then it's in Pango units rather than points. There
  // are PANGO_SCALE Pango units in a device unit (pixel).
  if (pango_font_description_get_size_is_absolute(pango_font))
    return pango_font_description_get_size(pango_font) / PANGO_SCALE;

  // Otherwise, we need to convert from points.
  return static_cast<int>(GetPixelsInPoint() *
      pango_font_description_get_size(pango_font) / PANGO_SCALE + 0.5);
}

PangoFontMetrics* GetPangoFontMetrics(PangoFontDescription* desc) {
  static std::map<int, PangoFontMetrics*>* desc_to_metrics = NULL;
  static PangoContext* context = NULL;

  if (!context) {
    context = GetPangoContext();
    pango_context_set_language(context, pango_language_get_default());
  }

  if (!desc_to_metrics)
    desc_to_metrics = new std::map<int, PangoFontMetrics*>();

  const int desc_hash = pango_font_description_hash(desc);
  std::map<int, PangoFontMetrics*>::iterator i =
      desc_to_metrics->find(desc_hash);

  if (i == desc_to_metrics->end()) {
    PangoFontMetrics* metrics = pango_context_get_metrics(context, desc, NULL);
    desc_to_metrics->insert(std::make_pair(desc_hash, metrics));
    return metrics;
  }
  return i->second;
}

}  // namespace gfx
