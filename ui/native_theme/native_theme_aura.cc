// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/native_theme/common_theme.h"

namespace {

const SkColor kMenuBackgroundColor = SK_ColorWHITE;

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::instance() {
  return NativeThemeAura::instance();
}

// static
NativeThemeAura* NativeThemeAura::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAura, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeAura::NativeThemeAura() {
  // We don't draw scrollbar buttons.
  set_scrollbar_button_length(0);
}

NativeThemeAura::~NativeThemeAura() {
}

void NativeThemeAura::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  if (menu_background.corner_radius > 0) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(kMenuBackgroundColor);

    gfx::Path path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, paint);
  } else {
    canvas->drawColor(kMenuBackgroundColor, SkXfermode::kSrc_Mode);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  CommonThemePaintMenuItemBackground(canvas, state, rect);
}

void NativeThemeAura::PaintScrollbarTrack(
    SkCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (part == kScrollbarVerticalTrack) {
    int center_offset = 0;
    int center_height = rect.height();

    if (rect.y() == extra_params.track_y) {
      // TODO(derat): Honor |state| instead of only using the highlighted images
      // after updating WebKit so we can draw the entire track in one go instead
      // of as two separate pieces: otherwise, only the portion of the scrollbar
      // that the mouse is over gets the highlighted state.
      gfx::ImageSkia* top = rb.GetImageSkiaNamed(
          IDR_SCROLL_BASE_VERTICAL_TOP_H);
      DrawTiledImage(canvas, *top,
                     0, 0, 1.0, 1.0,
                     rect.x(), rect.y(), top->width(), top->height());
      center_offset += top->height();
      center_height -= top->height();
    }

    if (rect.y() + rect.height() ==
        extra_params.track_y + extra_params.track_height) {
      gfx::ImageSkia* bottom = rb.GetImageSkiaNamed(
          IDR_SCROLL_BASE_VERTICAL_BOTTOM_H);
      DrawTiledImage(canvas, *bottom,
                     0, 0, 1.0, 1.0,
                     rect.x(), rect.y() + rect.height() - bottom->height(),
                     bottom->width(), bottom->height());
      center_height -= bottom->height();
    }

    if (center_height > 0) {
      gfx::ImageSkia* center = rb.GetImageSkiaNamed(
          IDR_SCROLL_BASE_VERTICAL_CENTER_H);
      DrawTiledImage(canvas, *center,
                     0, 0, 1.0, 1.0,
                     rect.x(), rect.y() + center_offset,
                     center->width(), center_height);
    }
  } else {
    int center_offset = 0;
    int center_width = rect.width();

    if (rect.x() == extra_params.track_x) {
      gfx::ImageSkia* left = rb.GetImageSkiaNamed(
          IDR_SCROLL_BASE_HORIZONTAL_LEFT_H);
      DrawTiledImage(canvas, *left,
                     0, 0, 1.0, 1.0,
                     rect.x(), rect.y(), left->width(), left->height());
      center_offset += left->width();
      center_width -= left->width();
    }

    if (rect.x() + rect.width() ==
        extra_params.track_x + extra_params.track_width) {
      gfx::ImageSkia* right = rb.GetImageSkiaNamed(
          IDR_SCROLL_BASE_HORIZONTAL_RIGHT_H);
      DrawTiledImage(canvas, *right,
                     0, 0, 1.0, 1.0,
                     rect.x() + rect.width() - right->width(), rect.y(),
                     right->width(), right->height());
      center_width -= right->width();
    }

    if (center_width > 0) {
      gfx::ImageSkia* center = rb.GetImageSkiaNamed(
          IDR_SCROLL_BASE_HORIZONTAL_CENTER_H);
      DrawTiledImage(canvas, *center,
                     0, 0, 1.0, 1.0,
                     rect.x() + center_offset, rect.y(),
                     center_width, center->height());
    }
  }
}

void NativeThemeAura::PaintScrollbarThumb(SkCanvas* canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (part == kScrollbarVerticalThumb) {
    int top_resource_id =
        state == kHovered ? IDR_SCROLL_THUMB_VERTICAL_TOP_H :
        state == kPressed ? IDR_SCROLL_THUMB_VERTICAL_TOP_P :
        IDR_SCROLL_THUMB_VERTICAL_TOP;
    gfx::ImageSkia* top = rb.GetImageSkiaNamed(top_resource_id);
    DrawTiledImage(canvas, *top,
                   0, 0, 1.0, 1.0,
                   rect.x(), rect.y(), top->width(), top->height());

    int bottom_resource_id =
        state == kHovered ? IDR_SCROLL_THUMB_VERTICAL_BOTTOM_H :
        state == kPressed ? IDR_SCROLL_THUMB_VERTICAL_BOTTOM_P :
        IDR_SCROLL_THUMB_VERTICAL_BOTTOM;
    gfx::ImageSkia* bottom = rb.GetImageSkiaNamed(bottom_resource_id);
    DrawTiledImage(canvas, *bottom,
                   0, 0, 1.0, 1.0,
                   rect.x(), rect.y() + rect.height() - bottom->height(),
                   bottom->width(), bottom->height());

    if (rect.height() > top->height() + bottom->height()) {
      int center_resource_id =
          state == kHovered ? IDR_SCROLL_THUMB_VERTICAL_CENTER_H :
          state == kPressed ? IDR_SCROLL_THUMB_VERTICAL_CENTER_P :
          IDR_SCROLL_THUMB_VERTICAL_CENTER;
      gfx::ImageSkia* center = rb.GetImageSkiaNamed(center_resource_id);
      DrawTiledImage(canvas, *center,
                     0, 0, 1.0, 1.0,
                     rect.x(), rect.y() + top->height(),
                     center->width(),
                     rect.height() - top->height() - bottom->height());
    }
  } else {
    int left_resource_id =
        state == kHovered ? IDR_SCROLL_THUMB_HORIZONTAL_LEFT_H :
        state == kPressed ? IDR_SCROLL_THUMB_HORIZONTAL_LEFT_P :
        IDR_SCROLL_THUMB_HORIZONTAL_LEFT;
    gfx::ImageSkia* left = rb.GetImageSkiaNamed(left_resource_id);
    DrawTiledImage(canvas, *left,
                   0, 0, 1.0, 1.0,
                   rect.x(), rect.y(), left->width(), left->height());

    int right_resource_id =
        state == kHovered ? IDR_SCROLL_THUMB_HORIZONTAL_RIGHT_H :
        state == kPressed ? IDR_SCROLL_THUMB_HORIZONTAL_RIGHT_P :
        IDR_SCROLL_THUMB_HORIZONTAL_RIGHT;
    gfx::ImageSkia* right = rb.GetImageSkiaNamed(right_resource_id);
    DrawTiledImage(canvas, *right,
                   0, 0, 1.0, 1.0,
                   rect.x() + rect.width() - right->width(), rect.y(),
                   right->width(), right->height());

    if (rect.width() > left->width() + right->width()) {
      int center_resource_id =
          state == kHovered ? IDR_SCROLL_THUMB_HORIZONTAL_CENTER_H :
          state == kPressed ? IDR_SCROLL_THUMB_HORIZONTAL_CENTER_P :
          IDR_SCROLL_THUMB_HORIZONTAL_CENTER;
      gfx::ImageSkia* center = rb.GetImageSkiaNamed(center_resource_id);
      DrawTiledImage(canvas, *center,
                     0, 0, 1.0, 1.0,
                     rect.x() + left->width(), rect.y(),
                     rect.width() - left->width() - right->width(),
                     center->height());
    }
  }
}

}  // namespace ui
