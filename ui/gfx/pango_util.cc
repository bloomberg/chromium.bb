// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/pango_util.h"

#include <cairo/cairo.h>
#include <fontconfig/fontconfig.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/platform_font_pango.h"
#include "ui/gfx/rect.h"

#if defined(TOOLKIT_GTK)
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "ui/gfx/gtk_util.h"
#else
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#endif

#include "ui/gfx/skia_util.h"

namespace {

// Marker for accelerators in the text.
const gunichar kAcceleratorChar = '&';

// Multiply by the text height to determine how much text should be faded
// when elliding.
const double kFadeWidthFactor = 1.5;

// End state of the elliding fade.
const double kFadeFinalAlpha = 0.15;

// Return |cairo_font_options|. If needed, allocate and update it.
cairo_font_options_t* GetCairoFontOptions() {
  // Font settings that we initialize once and then use when drawing text.
  static cairo_font_options_t* cairo_font_options = NULL;

  if (cairo_font_options)
    return cairo_font_options;

  cairo_font_options = cairo_font_options_create();

#if defined(TOOLKIT_GTK)
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;

  GtkSettings* gtk_settings = gtk_settings_get_default();
  g_object_get(gtk_settings,
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba_style,
               NULL);

  // g_object_get() doesn't tell us whether the properties were present or not,
  // but if they aren't (because gnome-settings-daemon isn't running), we'll get
  // NULL values for the strings.
  if (hint_style && rgba_style) {
    if (!antialias) {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_NONE);
    } else if (strcmp(rgba_style, "none") == 0) {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_GRAY);
    } else {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_SUBPIXEL);
      cairo_subpixel_order_t cairo_subpixel_order =
          CAIRO_SUBPIXEL_ORDER_DEFAULT;
      if (strcmp(rgba_style, "rgb") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
      } else if (strcmp(rgba_style, "bgr") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
      } else if (strcmp(rgba_style, "vrgb") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
      } else if (strcmp(rgba_style, "vbgr") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
      }
      cairo_font_options_set_subpixel_order(cairo_font_options,
                                            cairo_subpixel_order);
    }

    cairo_hint_style_t cairo_hint_style = CAIRO_HINT_STYLE_DEFAULT;
    if (hinting == 0 || strcmp(hint_style, "hintnone") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_NONE;
    } else if (strcmp(hint_style, "hintslight") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_SLIGHT;
    } else if (strcmp(hint_style, "hintmedium") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_MEDIUM;
    } else if (strcmp(hint_style, "hintfull") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_FULL;
    }
    cairo_font_options_set_hint_style(cairo_font_options, cairo_hint_style);
  }

  if (hint_style)
    g_free(hint_style);
  if (rgba_style)
    g_free(rgba_style);
#else
  // For non-GTK builds (read: Aura), use light hinting and fetch
  // subpixel-rendering settings from FontConfig.  We should really be getting
  // per-font settings here, but this path will be made obsolete by
  // http://crbug.com/105550.
  // TODO(derat): Create font_config_util.h/cc and move this there.
  FcPattern* pattern = FcPatternCreate();
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  DCHECK(match);
  int fc_rgba = FC_RGBA_RGB;
  FcPatternGetInteger(match, FC_RGBA, 0, &fc_rgba);
  FcPatternDestroy(pattern);
  FcPatternDestroy(match);

  cairo_antialias_t cairo_antialias = (fc_rgba != FC_RGBA_NONE) ?
      CAIRO_ANTIALIAS_SUBPIXEL : CAIRO_ANTIALIAS_GRAY;

  cairo_subpixel_order_t cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
  switch (fc_rgba) {
    case FC_RGBA_RGB:
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
      break;
    case FC_RGBA_BGR:
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
      break;
    case FC_RGBA_VRGB:
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
      break;
    case FC_RGBA_VBGR:
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
      break;
  }

  cairo_font_options_set_antialias(cairo_font_options, cairo_antialias);
  cairo_font_options_set_subpixel_order(cairo_font_options,
                                        cairo_subpixel_order);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTextSubpixelPositioning)) {
    // To enable subpixel positioning, we need to disable hinting.
    cairo_font_options_set_hint_metrics(cairo_font_options,
                                        CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_hint_style(cairo_font_options,
                                      CAIRO_HINT_STYLE_NONE);
  } else {
    cairo_font_options_set_hint_metrics(cairo_font_options,
                                        CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_hint_style(cairo_font_options,
                                      CAIRO_HINT_STYLE_SLIGHT);
  }
#endif

  return cairo_font_options;
}

// Returns the number of pixels in a point.
// - multiply a point size by this to get pixels ("device units")
// - divide a pixel size by this to get points
float GetPixelsInPoint() {
  static float pixels_in_point = 1.0;
  static bool determined_value = false;

  if (!determined_value) {
    // http://goo.gl/UIh5m: "This is a scale factor between points specified in
    // a PangoFontDescription and Cairo units.  The default value is 96, meaning
    // that a 10 point font will be 13 units high. (10 * 96. / 72. = 13.3)."
    double pango_dpi = gfx::GetPangoResolution();
    if (pango_dpi <= 0)
      pango_dpi = 96.0;
    pixels_in_point = pango_dpi / 72.0;  // 72 points in an inch
    determined_value = true;
  }

  return pixels_in_point;
}

}  // namespace

namespace gfx {

PangoContext* GetPangoContext() {
#if defined(USE_AURA)
  PangoFontMap* font_map = pango_cairo_font_map_get_default();
  return pango_font_map_create_context(font_map);
#else
  return gdk_pango_context_get();
#endif
}

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

void DrawTextOntoCairoSurface(cairo_t* cr,
                              const string16& text,
                              const gfx::Font& font,
                              const gfx::Rect& bounds,
                              const gfx::Rect& clip,
                              SkColor text_color,
                              int flags) {
  PangoLayout* layout = pango_cairo_create_layout(cr);
  base::i18n::TextDirection text_direction =
      base::i18n::GetFirstStrongCharacterDirection(text);
  Rect text_rect(bounds.x(), bounds.y(), 0, 0);
  DCHECK(!bounds.IsEmpty());

  gfx::SetupPangoLayout(
      layout, text, font, bounds.width(), text_direction, flags);

  pango_layout_set_height(layout, bounds.height() * PANGO_SCALE);

  cairo_save(cr);
  cairo_rectangle(cr, clip.x(), clip.y(), clip.width(), clip.height());
  cairo_clip(cr);

  AdjustTextRectBasedOnLayout(layout, bounds, flags, &text_rect);

  DrawPangoLayout(cr, layout, font, bounds, text_rect,
                  text_color, text_direction, flags);

  cairo_restore(cr);
  g_object_unref(layout);
}

// Pass a width greater than 0 to force wrapping and eliding.
static void SetupPangoLayoutWithoutFont(
    PangoLayout* layout,
    const string16& text,
    int width,
    base::i18n::TextDirection text_direction,
    int flags) {
  cairo_font_options_t* cairo_font_options = GetCairoFontOptions();

  // If we got an explicit request to turn off subpixel rendering, disable it on
  // a copy of the static font options object.
  bool copied_cairo_font_options = false;
  if ((flags & Canvas::NO_SUBPIXEL_RENDERING) &&
      (cairo_font_options_get_antialias(cairo_font_options) ==
       CAIRO_ANTIALIAS_SUBPIXEL)) {
    cairo_font_options = cairo_font_options_copy(cairo_font_options);
    copied_cairo_font_options = true;
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_GRAY);
  }

  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);

  if (copied_cairo_font_options) {
    cairo_font_options_destroy(cairo_font_options);
    cairo_font_options = NULL;
  }

  // Callers of DrawStringInt handle RTL layout themselves, so tell pango to not
  // scope out RTL characters.
  pango_layout_set_auto_dir(layout, FALSE);

  if (width > 0)
    pango_layout_set_width(layout, width * PANGO_SCALE);

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

  // Set the resolution to match that used by Gtk. If we don't set the
  // resolution and the resolution differs from the default, Gtk and Chrome end
  // up drawing at different sizes.
  double resolution = GetPangoResolution();
  if (resolution > 0) {
    pango_cairo_context_set_resolution(pango_layout_get_context(layout),
                                       resolution);
  }

  // Set text and accelerator character if needed.
  if (flags & Canvas::SHOW_PREFIX) {
    // Escape the text string to be used as markup.
    std::string utf8 = UTF16ToUTF8(text);
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
      string16 accelerator_removed =
          RemoveAcceleratorChar(text, static_cast<char16>(kAcceleratorChar),
                                NULL, NULL);
      utf8 = UTF16ToUTF8(accelerator_removed);
    } else {
      utf8 = UTF16ToUTF8(text);
    }

    pango_layout_set_text(layout, utf8.data(), utf8.size());
  }
}

void SetupPangoLayout(PangoLayout* layout,
                      const string16& text,
                      const Font& font,
                      int width,
                      base::i18n::TextDirection text_direction,
                      int flags) {
  SetupPangoLayoutWithoutFont(layout, text, width, text_direction, flags);

  PangoFontDescription* desc = font.GetNativeFont();
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);
}

void SetupPangoLayoutWithFontDescription(
    PangoLayout* layout,
    const string16& text,
    const std::string& font_description,
    int width,
    base::i18n::TextDirection text_direction,
    int flags) {
  SetupPangoLayoutWithoutFont(layout, text, width, text_direction, flags);

  PangoFontDescription* desc = pango_font_description_from_string(
      font_description.c_str());
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);
}

void AdjustTextRectBasedOnLayout(PangoLayout* layout,
                                 const gfx::Rect& bounds,
                                 int flags,
                                 gfx::Rect* text_rect) {
  int text_width, text_height;
  pango_layout_get_pixel_size(layout, &text_width, &text_height);
  text_rect->set_width(text_width);
  text_rect->set_height(text_height);

  if (flags & gfx::Canvas::TEXT_VALIGN_TOP) {
    // Cairo should draw from the top left corner already.
  } else if (flags & gfx::Canvas::TEXT_VALIGN_BOTTOM) {
    text_rect->set_y(text_rect->y() + bounds.height() - text_rect->height());
  } else {
    // Vertically centered.
    text_rect->set_y(text_rect->y() +
                     ((bounds.height() - text_rect->height()) / 2));
  }
}

void DrawPangoLayout(cairo_t* cr,
                     PangoLayout* layout,
                     const Font& font,
                     const gfx::Rect& bounds,
                     const gfx::Rect& text_rect,
                     SkColor text_color,
                     base::i18n::TextDirection text_direction,
                     int flags) {
  double r = SkColorGetR(text_color) / 255.0,
         g = SkColorGetG(text_color) / 255.0,
         b = SkColorGetB(text_color) / 255.0,
         a = SkColorGetA(text_color) / 255.0;

  cairo_pattern_t* pattern = NULL;

  cairo_save(cr);

  // If we're not eliding, use a fixed color.
  // Otherwise, create a gradient pattern to use as the source.
  if (text_direction == base::i18n::RIGHT_TO_LEFT ||
      (flags & gfx::Canvas::NO_ELLIPSIS) ||
      text_rect.width() <= bounds.width()) {
    cairo_set_source_rgba(cr, r, g, b, a);
  } else {
    // Fade to semi-transparent to elide.
    int fade_width = static_cast<double>(text_rect.height()) * kFadeWidthFactor;
    if (fade_width > bounds.width() / 2) {
      // Don't fade more than half the text.
      fade_width = bounds.width() / 2;
    }
    int fade_x = bounds.x() + bounds.width() - fade_width;

    pattern = cairo_pattern_create_linear(
        fade_x, bounds.y(), bounds.x() + bounds.width(), bounds.y());
    cairo_pattern_add_color_stop_rgba(pattern, 0, r, g, b, a);
    cairo_pattern_add_color_stop_rgba(pattern, 1, r, g, b, kFadeFinalAlpha);
    cairo_set_source(cr, pattern);
  }

  cairo_move_to(cr, text_rect.x(), text_rect.y());
  pango_cairo_show_layout(cr, layout);

  if (font.GetStyle() & gfx::Font::UNDERLINED) {
    gfx::PlatformFontPango* platform_font =
        static_cast<gfx::PlatformFontPango*>(font.platform_font());
    DrawPangoTextUnderline(cr, platform_font, 0.0, text_rect);
  }

  if (pattern)
    cairo_pattern_destroy(pattern);

  cairo_restore(cr);
}

void DrawPangoTextUnderline(cairo_t* cr,
                            gfx::PlatformFontPango* platform_font,
                            double extra_edge_width,
                            const Rect& text_rect) {
  const double underline_y =
      static_cast<double>(text_rect.y()) + text_rect.height() +
      platform_font->underline_position();
  cairo_set_line_width(
      cr, platform_font->underline_thickness() + 2 * extra_edge_width);
  cairo_move_to(cr,
                text_rect.x() - extra_edge_width,
                underline_y);
  cairo_line_to(cr,
                text_rect.x() + text_rect.width() + extra_edge_width,
                underline_y);
  cairo_stroke(cr);
}

size_t GetPangoFontSizeInPixels(PangoFontDescription* pango_font) {
  size_t size_in_pixels = pango_font_description_get_size(pango_font);
  if (pango_font_description_get_size_is_absolute(pango_font)) {
    // If the size is absolute, then it's in Pango units rather than points.
    // There are PANGO_SCALE Pango units in a device unit (pixel).
    size_in_pixels /= PANGO_SCALE;
  } else {
    // Otherwise, we need to convert from points.
    size_in_pixels = size_in_pixels * GetPixelsInPoint() / PANGO_SCALE;
  }
  return size_in_pixels;
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
