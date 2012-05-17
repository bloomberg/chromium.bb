// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_bubble_border.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkBlurDrawLooper.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"

namespace {

// Bubble border corner radius.
const int kCornerRadius = 3;

// Arrow width and height.
const int kArrowHeight = 10;
const int kArrowWidth = 20;

// Bubble border color and width.
const SkColor kBorderColor = SkColorSetARGB(0xFF, 0, 0, 0);
const int kBorderSize = 1;

// Bubble shadow color and radius.
const SkColor kShadowColor = SkColorSetARGB(0xFF, 0, 0, 0);
const int kShadowRadius = 4;

// Colors and sizes of top separator between searchbox and grid view.
const SkColor kTopSeparatorColor = SkColorSetRGB(0xDB, 0xDB, 0xDB);
const int kTopSeparatorSize = 1;
const SkColor kTopSeparatorGradientColor1 = SkColorSetRGB(0xEF, 0xEF, 0xEF);
const SkColor kTopSeparatorGradientColor2 = SkColorSetRGB(0xF9, 0xF9, 0xF9);
const int kTopSeparatorGradientSize = 9;

// Colors and sizes of bottom separator bwtween grid view and page switcher.
const SkColor kFooterBorderGradientColor1 = SkColorSetRGB(0x9F, 0x9F, 0x9F);
const SkColor kFooterBorderGradientColor2 = SkColorSetRGB(0xD9, 0xD9, 0xD9);
const int kFooterBorderSize = 3;
const SkColor kFooterBackground = SkColorSetRGB(0xD9, 0xD9, 0xD9);

// TODO(xiyuan): Merge this with the one in skia_util.
SkShader* CreateVerticalGradientShader(int start_point,
                                       int end_point,
                                       SkColor start_color,
                                       SkColor end_color,
                                       SkShader::TileMode mode) {
  SkColor grad_colors[2] = { start_color, end_color};
  SkPoint grad_points[2];
  grad_points[0].iset(0, start_point);
  grad_points[1].iset(0, end_point);

  return SkGradientShader::CreateLinear(grad_points,
                                        grad_colors,
                                        NULL,
                                        2,
                                        mode);
}

// Builds a bubble shape for given |bounds|.
void BuildShape(const gfx::Rect& bounds,
                SkScalar padding,
                SkScalar arrow_offset,
                SkPath* path) {
  const SkScalar left = SkIntToScalar(bounds.x()) + padding;
  const SkScalar top = SkIntToScalar(bounds.y()) + padding;
  const SkScalar right = SkIntToScalar(bounds.right()) - padding;
  const SkScalar bottom = SkIntToScalar(bounds.bottom()) - padding;

  const SkScalar center_x = SkIntToScalar((bounds.x() + bounds.right()) / 2);
  const SkScalar center_y = SkIntToScalar((bounds.y() + bounds.bottom()) / 2);

  const SkScalar half_array_width = SkIntToScalar(kArrowWidth / 2);
  const SkScalar arrow_height = SkIntToScalar(kArrowHeight) - padding;

  path->reset();
  path->incReserve(12);

  path->moveTo(center_x, top);
  path->arcTo(left, top, left, center_y, SkIntToScalar(kCornerRadius));
  path->arcTo(left, bottom, center_x  - half_array_width, bottom,
              SkIntToScalar(kCornerRadius));
  path->lineTo(center_x + arrow_offset - half_array_width, bottom);
  path->lineTo(center_x + arrow_offset, bottom + arrow_height);
  path->lineTo(center_x + arrow_offset + half_array_width, bottom);
  path->arcTo(right, bottom, right, center_y, SkIntToScalar(kCornerRadius));
  path->arcTo(right, top, center_x, top, SkIntToScalar(kCornerRadius));
  path->close();
}

}  // namespace

namespace app_list {

AppListBubbleBorder::AppListBubbleBorder(views::View* app_list_view)
    : views::BubbleBorder(views::BubbleBorder::BOTTOM_RIGHT,
                          views::BubbleBorder::NO_SHADOW),
      app_list_view_(app_list_view),
      arrow_offset_(0) {
}

AppListBubbleBorder::~AppListBubbleBorder() {
}

void AppListBubbleBorder::PaintModelViewBackground(
    gfx::Canvas* canvas,
    const gfx::Rect& bounds) const {
  const views::View* page_switcher = app_list_view_->child_at(1);
  const gfx::Rect page_switcher_bounds =
      app_list_view_->ConvertRectToWidget(page_switcher->bounds());
  gfx::Rect rect(bounds.x(),
                 bounds.y(),
                 bounds.width(),
                 page_switcher_bounds.y() - bounds.y());

  // TODO(xiyuan): Draw 1px separator line after SearchBoxView is added.

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkSafeUnref(paint.setShader(CreateVerticalGradientShader(
      rect.y(),
      rect.y() + kTopSeparatorGradientSize,
      kTopSeparatorGradientColor1,
      kTopSeparatorGradientColor2,
      SkShader::kClamp_TileMode)));
  canvas->DrawRect(rect, paint);
}

void AppListBubbleBorder::PaintPageSwitcherBackground(
    gfx::Canvas* canvas,
    const gfx::Rect& bounds) const {
  const views::View* page_switcher = app_list_view_->child_at(1);
  const gfx::Rect page_switcher_bounds =
      app_list_view_->ConvertRectToWidget(page_switcher->bounds());

  gfx::Rect rect(bounds.x(),
                 page_switcher_bounds.y(),
                 bounds.width(),
                 kFooterBorderSize);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkSafeUnref(paint.setShader(CreateVerticalGradientShader(
      rect.y(),
      rect.bottom(),
      kFooterBorderGradientColor1,
      kFooterBorderGradientColor2,
      SkShader::kClamp_TileMode)));
  canvas->DrawRect(rect, paint);

  rect.set_y(rect.bottom());
  rect.set_height(bounds.bottom() - rect.y() + kArrowHeight - kBorderSize);
  canvas->FillRect(rect, kFooterBackground);
}

void AppListBubbleBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kShadowRadius + kBorderSize,
              kShadowRadius + kBorderSize,
              kShadowRadius + kBorderSize + kArrowHeight,
              kShadowRadius + kBorderSize);
}

gfx::Rect AppListBubbleBorder::GetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size) const {
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.width(), insets.height());

  int anchor_x = (position_relative_to.x() + position_relative_to.right()) / 2;
  int arrow_tip_x = border_size.width() / 2 + arrow_offset_;

  return gfx::Rect(
      gfx::Point(anchor_x - arrow_tip_x,
                 position_relative_to.y() - border_size.height() +
                     kShadowRadius),
      border_size);
}

void AppListBubbleBorder::Paint(const views::View& view,
                                gfx::Canvas* canvas) const {
  gfx::Insets insets;
  GetInsets(&insets);

  gfx::Rect bounds = view.bounds();
  bounds.Inset(insets);

  SkPath path;

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(kBorderColor);
  SkSafeUnref(paint.setLooper(
      new SkBlurDrawLooper(kShadowRadius,
                           0, 0,
                           kShadowColor,
                           SkBlurDrawLooper::kHighQuality_BlurFlag)));
  // Pads with 0.5 pixel since anti alias is used.
  BuildShape(bounds,
             SkDoubleToScalar(0.5),
             SkIntToScalar(arrow_offset_),
             &path);
  canvas->DrawPath(path, paint);

  // Pads with kBoprderSize pixels to leave space for border lines.
  BuildShape(bounds,
             SkIntToScalar(kBorderSize),
             SkIntToScalar(arrow_offset_),
             &path);
  canvas->Save();
  canvas->ClipPath(path);

  PaintModelViewBackground(canvas, bounds);
  PaintPageSwitcherBackground(canvas, bounds);

  canvas->Restore();
}

}  // namespace app_list
