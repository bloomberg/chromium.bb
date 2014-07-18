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

// Marker for accelerators in the text.
const gunichar kAcceleratorChar = '&';

// Creates and returns a PangoContext. The caller owns the context.
PangoContext* GetPangoContext() {
  PangoFontMap* font_map = pango_cairo_font_map_get_default();
  return pango_font_map_create_context(font_map);
}

// Creates a new cairo_font_options_t based on |params|.
cairo_font_options_t* CreateCairoFontOptions(const FontRenderParams& params) {
  cairo_font_options_t* cairo_font_options = cairo_font_options_create();

  FontRenderParams::SubpixelRendering subpixel = params.subpixel_rendering;
  if (!params.antialiasing) {
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_NONE);
  } else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_NONE) {
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_GRAY);
  } else {
    cairo_font_options_set_antialias(cairo_font_options,
                                     CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_subpixel_order_t cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_RGB)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
    else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_BGR)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
    else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_VRGB)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
    else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_VBGR)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
    else
      NOTREACHED() << "Unhandled subpixel rendering type " << subpixel;
    cairo_font_options_set_subpixel_order(cairo_font_options,
                                          cairo_subpixel_order);
  }

  if (params.hinting == FontRenderParams::HINTING_NONE ||
      params.subpixel_positioning) {
    cairo_font_options_set_hint_style(cairo_font_options,
                                      CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(cairo_font_options,
                                        CAIRO_HINT_METRICS_OFF);
  } else {
    cairo_hint_style_t cairo_hint_style = CAIRO_HINT_STYLE_DEFAULT;
    if (params.hinting == FontRenderParams::HINTING_SLIGHT)
      cairo_hint_style = CAIRO_HINT_STYLE_SLIGHT;
    else if (params.hinting == FontRenderParams::HINTING_MEDIUM)
      cairo_hint_style = CAIRO_HINT_STYLE_MEDIUM;
    else if (params.hinting == FontRenderParams::HINTING_FULL)
      cairo_hint_style = CAIRO_HINT_STYLE_FULL;
    else
      NOTREACHED() << "Unhandled hinting style " << params.hinting;
    cairo_font_options_set_hint_style(cairo_font_options, cairo_hint_style);
    cairo_font_options_set_hint_metrics(cairo_font_options,
                                        CAIRO_HINT_METRICS_ON);
  }

  return cairo_font_options;
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

void SetUpPangoLayout(
    PangoLayout* layout,
    const base::string16& text,
    const FontList& font_list,
    base::i18n::TextDirection text_direction,
    int flags) {
  cairo_font_options_t* cairo_font_options = CreateCairoFontOptions(
      font_list.GetPrimaryFont().GetFontRenderParams());

  // If we got an explicit request to turn off subpixel rendering, disable it.
  if ((flags & Canvas::NO_SUBPIXEL_RENDERING) &&
      (cairo_font_options_get_antialias(cairo_font_options) ==
       CAIRO_ANTIALIAS_SUBPIXEL))
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_GRAY);

  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);
  cairo_font_options_destroy(cairo_font_options);
  cairo_font_options = NULL;

  // Set Pango's base text direction explicitly from |text_direction|.
  pango_layout_set_auto_dir(layout, FALSE);
  pango_context_set_base_dir(pango_layout_get_context(layout),
      (text_direction == base::i18n::RIGHT_TO_LEFT ?
       PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR));

  if (flags & Canvas::TEXT_ALIGN_CENTER) {
    // We don't support center aligned w/ eliding.
    DCHECK(gfx::Canvas::NO_ELLIPSIS);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & Canvas::TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & Canvas::NO_ELLIPSIS) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    if (flags & Canvas::MULTI_LINE) {
      pango_layout_set_wrap(layout,
          (flags & Canvas::CHARACTER_BREAK) ?
              PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
    }
  } else if (text_direction == base::i18n::RIGHT_TO_LEFT) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  } else {
    // Fading the text will be handled in the draw operation.
    // Ensure that the text is only on one line.
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    pango_layout_set_width(layout, -1);
  }

  // Set the layout's resolution to match the resolution used to convert from
  // points to pixels.
  pango_cairo_context_set_resolution(pango_layout_get_context(layout),
                                     GetPangoDPI());

  // Set text and accelerator character if needed.
  if (flags & Canvas::SHOW_PREFIX) {
    // Escape the text string to be used as markup.
    std::string utf8 = base::UTF16ToUTF8(text);
    gchar* escaped_text = g_markup_escape_text(utf8.c_str(), utf8.size());
    pango_layout_set_markup_with_accel(layout,
                                       escaped_text,
                                       strlen(escaped_text),
                                       kAcceleratorChar, NULL);
    g_free(escaped_text);
  } else {
    std::string utf8;

    // Remove the ampersand character.  A double ampersand is output as
    // a single ampersand.
    if (flags & Canvas::HIDE_PREFIX) {
      DCHECK_EQ(1, g_unichar_to_utf8(kAcceleratorChar, NULL));
      base::string16 accelerator_removed =
          RemoveAcceleratorChar(text,
                                static_cast<base::char16>(kAcceleratorChar),
                                NULL, NULL);
      utf8 = base::UTF16ToUTF8(accelerator_removed);
    } else {
      utf8 = base::UTF16ToUTF8(text);
    }

    pango_layout_set_text(layout, utf8.data(), utf8.size());
  }

  ScopedPangoFontDescription desc(pango_font_description_from_string(
      font_list.GetFontDescriptionString().c_str()));
  pango_layout_set_font_description(layout, desc.get());
}

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
