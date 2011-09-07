// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas_skia.h"

#include <algorithm>

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/pango_util.h"
#include "ui/gfx/platform_font_gtk.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

using std::max;

namespace {

// Multiply by the text height to determine how much text should be faded
// when elliding.
const double kFadeWidthFactor = 1.5;

// End state of the elliding fade.
const double kFadeFinalAlpha = 0.15;

// Width of the border drawn around haloed text.
const double kTextHaloWidth = 1.0;

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
  void DrawWithHalo(const SkColor& text_color, const SkColor& halo_color);

 private:
  // Draw an underline under the text using |cr|, which must already be
  // initialized with the correct source.  |extra_edge_width| is added to the
  // outer edge of the line.  Helper method for Draw() and DrawWithHalo().
  void DrawUnderline(cairo_t* cr, double extra_edge_width);

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

  base::i18n::TextDirection text_direction_;

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
      text_height_(0),
      text_direction_(base::i18n::GetFirstStrongCharacterDirection(text)) {
  DCHECK(!bounds_.IsEmpty());

  cr_ = skia::BeginPlatformPaint(canvas_);
  layout_ = pango_cairo_create_layout(cr_);

  gfx::SetupPangoLayout(
      layout_, text, font, bounds_.width(), text_direction_, flags_);

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
  cairo_restore(cr_);
  skia::EndPlatformPaint(canvas_);
  g_object_unref(layout_);
  // NOTE: BeginPlatformPaint returned its surface, we shouldn't destroy it.
}

void DrawStringContext::Draw(const SkColor& text_color) {
  double r = SkColorGetR(text_color) / 255.0,
         g = SkColorGetG(text_color) / 255.0,
         b = SkColorGetB(text_color) / 255.0,
         a = SkColorGetA(text_color) / 255.0;

  cairo_pattern_t* pattern = NULL;

  cairo_save(cr_);

  // If we're not eliding, use a fixed color.
  // Otherwise, create a gradient pattern to use as the source.
  if (text_direction_ == base::i18n::RIGHT_TO_LEFT ||
      (flags_ & gfx::Canvas::NO_ELLIPSIS) ||
      text_width_ <= bounds_.width()) {
    cairo_set_source_rgba(cr_, r, g, b, a);
  } else {
    // Fade to semi-transparent to elide.
    int fade_width = static_cast<double>(text_height_) * kFadeWidthFactor;
    if (fade_width > (bounds_.width() / 2)) {
      // Don't fade more than half the text.
      fade_width = bounds_.width() / 2;
    }
    int fade_x = bounds_.x() + bounds_.width() - fade_width;

    pattern = cairo_pattern_create_linear(
        fade_x, bounds_.y(), bounds_.x() + bounds_.width(), bounds_.y());
    cairo_pattern_add_color_stop_rgba(pattern, 0, r, g, b, a);
    cairo_pattern_add_color_stop_rgba(pattern, 1, r, g, b, kFadeFinalAlpha);
    cairo_set_source(cr_, pattern);
  }

  cairo_move_to(cr_, text_x_, text_y_);
  pango_cairo_show_layout(cr_, layout_);

  if (font_.GetStyle() & gfx::Font::UNDERLINED)
    DrawUnderline(cr_, 0.0);

  if (pattern)
    cairo_pattern_destroy(pattern);

  cairo_restore(cr_);
}

void DrawStringContext::DrawWithHalo(const SkColor& text_color,
                                     const SkColor& halo_color) {
  gfx::CanvasSkia text_canvas(bounds_.width() + 2, bounds_.height() + 2, false);
  text_canvas.FillRectInt(static_cast<SkColor>(0),
      0, 0, bounds_.width() + 2, bounds_.height() + 2);

  {
    skia::ScopedPlatformPaint scoped_platform_paint(&text_canvas);
    cairo_t* text_cr = scoped_platform_paint.GetPlatformSurface();

    // TODO: The current approach (stroking the text path to generate the halo
    // and then filling it for the main text) won't work if |text_color| is
    // non-opaque.  If we need to do this at some later point,
    // http://lists.freedesktop.org/archives/cairo/2004-September/001829.html
    // suggests "do[ing] the stroke and fill with opaque paint onto an
    // intermediate surface, and then us[ing] cairo_show_surface to composite
    // that intermediate result with the desired compositing operator."
    cairo_set_source_rgba(text_cr,
                          SkColorGetR(halo_color) / 255.0,
                          SkColorGetG(halo_color) / 255.0,
                          SkColorGetB(halo_color) / 255.0,
                          SkColorGetA(halo_color) / 255.0);

    // Draw the halo underline first so that we can use the same path for the
    // outer/halo text and inner text.
    if (font_.GetStyle() & gfx::Font::UNDERLINED)
      DrawUnderline(text_cr, kTextHaloWidth);

    cairo_move_to(text_cr, 2, 1);
    pango_cairo_layout_path(text_cr, layout_);
    cairo_set_line_width(text_cr, 2 * kTextHaloWidth);
    cairo_set_line_join(text_cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke_preserve(text_cr);

    cairo_set_operator(text_cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(text_cr,
                          SkColorGetR(text_color) / 255.0,
                          SkColorGetG(text_color) / 255.0,
                          SkColorGetB(text_color) / 255.0,
                          SkColorGetA(text_color) / 255.0);
    cairo_fill(text_cr);

    if (font_.GetStyle() & gfx::Font::UNDERLINED)
      DrawUnderline(text_cr, 0.0);
  }

  const SkBitmap& text_bitmap = const_cast<SkBitmap&>(
      skia::GetTopDevice(text_canvas)->accessBitmap(false));
  canvas_->DrawBitmapInt(text_bitmap, text_x_ - 1, text_y_ - 1);
}

void DrawStringContext::DrawUnderline(cairo_t* cr, double extra_edge_width) {
  gfx::PlatformFontGtk* platform_font =
      static_cast<gfx::PlatformFontGtk*>(font_.platform_font());
  const double underline_y =
      static_cast<double>(text_y_) + text_height_ +
      platform_font->underline_position();
  cairo_set_line_width(
      cr, platform_font->underline_thickness() + 2 * extra_edge_width);
  cairo_move_to(cr, text_x_ - extra_edge_width, underline_y);
  cairo_line_to(cr, text_x_ + text_width_ + extra_edge_width, underline_y);
  cairo_stroke(cr);
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

  SetupPangoLayout(
      layout,
      text,
      font,
      *width,
      base::i18n::GetFirstStrongCharacterDirection(text),
      flags);

  pango_layout_get_pixel_size(layout, width, height);

  if (font.GetStyle() & gfx::Font::UNDERLINED) {
    gfx::PlatformFontGtk* platform_font =
        static_cast<gfx::PlatformFontGtk*>(font.platform_font());
    *height += max(platform_font->underline_position() +
                   platform_font->underline_thickness(), 0.0);
  }

  // TODO: If the text is being drawn with a halo, we should also pad each of
  // the edges by |kTextHaloWidth|... except haloing is currently a drawing-time
  // thing, and we don't know how the text will be drawn here. :-(  This only
  // seems to come into play at present if the text is both haloed and
  // underlined; otherwise, the size returned by Pango is (at least sometimes)
  // large enough to include the halo.

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
  if (!IntersectsClipRectInt(x, y, w, h))
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
  if (!IntersectsClipRectInt(x, y, w, h))
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

  skia::ScopedPlatformPaint scoped_platform_paint(this);
  cairo_t* cr = scoped_platform_paint.GetPlatformSurface();
  gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
  cairo_paint(cr);
}

ui::TextureID CanvasSkia::GetTextureID() {
  // TODO(wjmaclean)
  return 0;
}

}  // namespace gfx
