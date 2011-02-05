// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas_skia.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/platform_font_gtk.h"
#include "ui/gfx/rect.h"

namespace {

const gunichar kAcceleratorChar = '&';

// Font settings that we initialize once and then use when drawing text in
// DrawStringInt().
static cairo_font_options_t* cairo_font_options = NULL;

// Update |cairo_font_options| based on GtkSettings, allocating it if needed.
static void UpdateCairoFontOptions() {
  if (!cairo_font_options)
    cairo_font_options = cairo_font_options_create();

  GtkSettings* gtk_settings = gtk_settings_get_default();
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;
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
}

// Pass a width > 0 to force wrapping and elliding.
static void SetupPangoLayout(PangoLayout* layout,
                             const string16& text,
                             const gfx::Font& font,
                             int width,
                             int flags) {
  if (!cairo_font_options)
    UpdateCairoFontOptions();
  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);

  // Callers of DrawStringInt handle RTL layout themselves, so tell pango to not
  // scope out RTL characters.
  pango_layout_set_auto_dir(layout, FALSE);

  if (width > 0)
    pango_layout_set_width(layout, width * PANGO_SCALE);

  if (flags & gfx::Canvas::NO_ELLIPSIS) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
  } else {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  }

  if (flags & gfx::Canvas::TEXT_ALIGN_CENTER) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & gfx::Canvas::TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & gfx::Canvas::MULTI_LINE) {
    pango_layout_set_wrap(layout,
        (flags & gfx::Canvas::CHARACTER_BREAK) ?
            PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
  }

  // Set the resolution to match that used by Gtk. If we don't set the
  // resolution and the resolution differs from the default, Gtk and Chrome end
  // up drawing at different sizes.
  double resolution = gfx::GetPangoResolution();
  if (resolution > 0) {
    pango_cairo_context_set_resolution(pango_layout_get_context(layout),
                                       resolution);
  }

  PangoFontDescription* desc = font.GetNativeFont();
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  // Set text and accelerator character if needed.
  std::string utf8 = UTF16ToUTF8(text);
  if (flags & gfx::Canvas::SHOW_PREFIX) {
    // Escape the text string to be used as markup.
    gchar* escaped_text = g_markup_escape_text(utf8.c_str(), utf8.size());
    pango_layout_set_markup_with_accel(layout,
                                       escaped_text,
                                       strlen(escaped_text),
                                       kAcceleratorChar, NULL);
    g_free(escaped_text);
  } else if (flags & gfx::Canvas::HIDE_PREFIX) {
    // Remove the ampersand character.
    utf8 = gfx::RemoveWindowsStyleAccelerators(utf8);
    pango_layout_set_text(layout, utf8.data(), utf8.size());
  } else {
    pango_layout_set_text(layout, utf8.data(), utf8.size());
  }
}

// A class to encapsulate string drawing params and operations.
class DrawStringContext {
 public:
  DrawStringContext(gfx::CanvasSkia* canvas,
                    const string16& text,
                    const gfx::Font& font,
                    const gfx::Rect& bounds,
                    const gfx::Rect& clip,
                    int flags);
  ~DrawStringContext();

  void Draw(const SkColor& text_color);
  void DrawWithHalo(const SkColor& text_color,
                          const SkColor& halo_color);

 private:
  const gfx::Rect& bounds_;
  int flags_;
  const gfx::Font& font_;

  gfx::CanvasSkia* canvas_;
  cairo_t* cr_;
  PangoLayout* layout_;

  int text_x_;
  int text_y_;
  int text_width_;
  int text_height_;

  DISALLOW_COPY_AND_ASSIGN(DrawStringContext);
};

DrawStringContext::DrawStringContext(gfx::CanvasSkia* canvas,
                                     const string16& text,
                                     const gfx::Font& font,
                                     const gfx::Rect& bounds,
                                     const gfx::Rect& clip,
                                     int flags)
    : bounds_(bounds),
      flags_(flags),
      font_(font),
      canvas_(canvas),
      cr_(NULL),
      layout_(NULL),
      text_x_(bounds.x()),
      text_y_(bounds.y()),
      text_width_(0),
      text_height_(0) {
  DCHECK(!bounds_.IsEmpty());

  cr_ = canvas_->beginPlatformPaint();
  layout_ = pango_cairo_create_layout(cr_);

  SetupPangoLayout(layout_, text, font, bounds_.width(), flags_);

  pango_layout_set_height(layout_, bounds_.height() * PANGO_SCALE);

  cairo_save(cr_);

  cairo_rectangle(cr_, clip.x(), clip.y(), clip.width(), clip.height());
  cairo_clip(cr_);

  pango_layout_get_pixel_size(layout_, &text_width_, &text_height_);

  if (flags_ & gfx::Canvas::TEXT_VALIGN_TOP) {
    // Cairo should draw from the top left corner already.
  } else if (flags_ & gfx::Canvas::TEXT_VALIGN_BOTTOM) {
    text_y_ += (bounds.height() - text_height_);
  } else {
    // Vertically centered.
    text_y_ += ((bounds.height() - text_height_) / 2);
  }
}

DrawStringContext::~DrawStringContext() {
  if (font_.GetStyle() & gfx::Font::UNDERLINED) {
    gfx::PlatformFontGtk* platform_font =
        static_cast<gfx::PlatformFontGtk*>(font_.platform_font());
    double underline_y =
        static_cast<double>(text_y_) + text_height_ +
        platform_font->underline_position();
    cairo_set_line_width(cr_, platform_font->underline_thickness());
    cairo_move_to(cr_, text_x_, underline_y);
    cairo_line_to(cr_, text_x_ + text_width_, underline_y);
    cairo_stroke(cr_);
  }
  cairo_restore(cr_);

  g_object_unref(layout_);
  // NOTE: beginPlatformPaint returned its surface, we shouldn't destroy it.
}

void DrawStringContext::Draw(const SkColor& text_color) {
  cairo_set_source_rgba(cr_,
                        SkColorGetR(text_color) / 255.0,
                        SkColorGetG(text_color) / 255.0,
                        SkColorGetB(text_color) / 255.0,
                        SkColorGetA(text_color) / 255.0);
  cairo_move_to(cr_, text_x_, text_y_);
  pango_cairo_show_layout(cr_, layout_);
}

void DrawStringContext::DrawWithHalo(const SkColor& text_color,
                                     const SkColor& halo_color) {
  gfx::CanvasSkia text_canvas(bounds_.width() + 2, bounds_.height() + 2, false);
  text_canvas.FillRectInt(static_cast<SkColor>(0),
      0, 0, bounds_.width() + 2, bounds_.height() + 2);

  cairo_t* text_cr = text_canvas.beginPlatformPaint();

  cairo_move_to(text_cr, 1, 1);
  pango_cairo_layout_path(text_cr, layout_);

  cairo_set_source_rgba(text_cr,
                        SkColorGetR(halo_color) / 255.0,
                        SkColorGetG(halo_color) / 255.0,
                        SkColorGetB(halo_color) / 255.0,
                        SkColorGetA(halo_color) / 255.0);
  cairo_set_line_width(text_cr, 2.0);
  cairo_set_line_join(text_cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke_preserve(text_cr);

  cairo_set_operator(text_cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(text_cr,
                        SkColorGetR(text_color) / 255.0,
                        SkColorGetG(text_color) / 255.0,
                        SkColorGetB(text_color) / 255.0,
                        SkColorGetA(text_color) / 255.0);
  cairo_fill(text_cr);

  text_canvas.endPlatformPaint();

  const SkBitmap& text_bitmap = const_cast<SkBitmap&>(
      text_canvas.getTopPlatformDevice().accessBitmap(false));
  canvas_->DrawBitmapInt(text_bitmap, text_x_ - 1, text_y_ - 1);
}

}  // namespace

namespace gfx {

CanvasSkia::CanvasSkia(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

CanvasSkia::CanvasSkia() : skia::PlatformCanvas() {
}

CanvasSkia::~CanvasSkia() {
}

// static
void CanvasSkia::SizeStringInt(const string16& text,
                               const gfx::Font& font,
                               int* width, int* height,
                               int flags) {
  int org_width = *width;
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
  cairo_t* cr = cairo_create(surface);
  PangoLayout* layout = pango_cairo_create_layout(cr);

  SetupPangoLayout(layout, text, font, *width, flags);

  pango_layout_get_pixel_size(layout, width, height);

  if (org_width > 0 && flags & Canvas::MULTI_LINE &&
      pango_layout_is_wrapped(layout)) {
    // The text wrapped. There seems to be a bug in Pango when this happens
    // such that the width returned from pango_layout_get_pixel_size is too
    // small. Using the width from pango_layout_get_pixel_size in this case
    // results in wrapping across more lines, which requires a bigger height.
    // As a workaround we use the original width, which is not necessarily
    // exactly correct, but isn't wrong by much.
    //
    // It looks like Pango uses the size of whitespace in calculating wrapping
    // but doesn't include the size of the whitespace when the extents are
    // asked for. See the loop in pango-layout.c process_item that determines
    // where to wrap.
    *width = org_width;
  }

  g_object_unref(layout);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void CanvasSkia::DrawStringWithHalo(const string16& text,
                                    const gfx::Font& font,
                                    const SkColor& text_color,
                                    const SkColor& halo_color,
                                    int x, int y, int w, int h,
                                    int flags) {
  if (w <= 0 || h <= 0)
    return;

  gfx::Rect bounds(x, y, w, h);
  gfx::Rect clip(x - 1, y - 1, w + 2, h + 2);  // Bigger clip for halo
  DrawStringContext context(this, text, font, bounds, clip,flags);
  context.DrawWithHalo(text_color, halo_color);
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               int x, int y, int w, int h,
                               int flags) {
  if (w <= 0 || h <= 0)
    return;

  gfx::Rect bounds(x, y, w, h);
  DrawStringContext context(this, text, font, bounds, bounds, flags);
  context.Draw(color);
}

void CanvasSkia::DrawGdkPixbuf(GdkPixbuf* pixbuf, int x, int y) {
  if (!pixbuf) {
    NOTREACHED();
    return;
  }

  cairo_t* cr = beginPlatformPaint();
  gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
  cairo_paint(cr);
}

}  // namespace gfx
