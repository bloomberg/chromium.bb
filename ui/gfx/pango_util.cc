// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/pango_util.h"

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"

#if !defined(USE_WAYLAND) && defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#include "ui/gfx/gtk_util.h"
#else
#include "ui/gfx/linux_util.h"
#endif

#include "ui/gfx/skia_util.h"

namespace {

// Marker for accelerators in the text.
const gunichar kAcceleratorChar = '&';

// Return |cairo_font_options|. If needed, allocate and update it based on
// GtkSettings.
cairo_font_options_t* GetCairoFontOptions() {
  // Font settings that we initialize once and then use when drawing text.
  static cairo_font_options_t* cairo_font_options = NULL;

  if (cairo_font_options)
    return cairo_font_options;

  cairo_font_options = cairo_font_options_create();

  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;

#if !defined(USE_WAYLAND) && defined(TOOLKIT_USES_GTK)
  // TODO(xji): still has gtk dependency.
  GtkSettings* gtk_settings = gtk_settings_get_default();
  g_object_get(gtk_settings,
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba_style,
               NULL);
#endif

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

  return cairo_font_options;
}

}  // namespace

namespace gfx {

// Pass a width greater than 0 to force wrapping and eliding.
void SetupPangoLayout(PangoLayout* layout,
                      const string16& text,
                      const Font& font,
                      int width,
                      base::i18n::TextDirection text_direction,
                      int flags) {
  cairo_font_options_t* cairo_font_options = GetCairoFontOptions();
  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);

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

  PangoFontDescription* desc = font.GetNativeFont();
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  // Set text and accelerator character if needed.
  std::string utf8 = UTF16ToUTF8(text);
  if (flags & Canvas::SHOW_PREFIX) {
    // Escape the text string to be used as markup.
    gchar* escaped_text = g_markup_escape_text(utf8.c_str(), utf8.size());
    pango_layout_set_markup_with_accel(layout,
                                       escaped_text,
                                       strlen(escaped_text),
                                       kAcceleratorChar, NULL);
    g_free(escaped_text);
  } else if (flags & Canvas::HIDE_PREFIX) {
    // Remove the ampersand character.  A double ampersand is output as
    // a single ampersand.
    DCHECK_EQ(1, g_unichar_to_utf8(kAcceleratorChar, NULL));
    const std::string accelerator_removed =
        RemoveAcceleratorChar(utf8, static_cast<char>(kAcceleratorChar));

    pango_layout_set_text(layout,
        accelerator_removed.data(), accelerator_removed.size());
  } else {
    pango_layout_set_text(layout, utf8.data(), utf8.size());
  }
}

}  // namespace gfx
