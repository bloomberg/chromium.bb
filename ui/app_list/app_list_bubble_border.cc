// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_bubble_border.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"

namespace {

// Bubble border corner radius.
const int kCornerRadius = 2;

// Arrow width and height.
const int kArrowHeight = 10;
const int kArrowWidth = 20;

// Bubble border color and width.
const SkColor kBorderColor = SkColorSetARGB(0x26, 0, 0, 0);
const int kBorderSize = 1;

const SkColor kSearchBoxBackground = SK_ColorWHITE;
const SkColor kContentsBackground = SkColorSetRGB(0xFC, 0xFC, 0xFC);

// Colors and sizes of top separator between searchbox and grid view.
const SkColor kTopSeparatorColor = SkColorSetRGB(0xF0, 0xF0, 0xF0);
const int kTopSeparatorSize = 1;

// Builds a bubble shape for given |bounds|.
void BuildShape(const gfx::Rect& bounds,
                views::BubbleBorder::ArrowLocation arrow_location,
                SkScalar arrow_offset,
                SkScalar padding,
                SkPath* path) {
  const SkScalar corner_radius = SkIntToScalar(kCornerRadius);

  const SkScalar left = SkIntToScalar(bounds.x()) + padding;
  const SkScalar top = SkIntToScalar(bounds.y()) + padding;
  const SkScalar right = SkIntToScalar(bounds.right()) - padding;
  const SkScalar bottom = SkIntToScalar(bounds.bottom()) - padding;

  const SkScalar center_x = SkIntToScalar((bounds.x() + bounds.right()) / 2);
  const SkScalar center_y = SkIntToScalar((bounds.y() + bounds.bottom()) / 2);

  const SkScalar half_arrow_width = SkIntToScalar(kArrowWidth / 2);
  const SkScalar arrow_height = SkIntToScalar(kArrowHeight) - padding;

  path->reset();
  path->incReserve(12);

  switch (arrow_location) {
    case views::BubbleBorder::TOP_LEFT:
    case views::BubbleBorder::TOP_RIGHT:
      path->moveTo(center_x, bottom);
      path->arcTo(right, bottom, right, center_y, corner_radius);
      path->arcTo(right, top, center_x  - half_arrow_width, top,
                  corner_radius);
      path->lineTo(center_x + arrow_offset + half_arrow_width, top);
      path->lineTo(center_x + arrow_offset, top - arrow_height);
      path->lineTo(center_x + arrow_offset - half_arrow_width, top);
      path->arcTo(left, top, left, center_y, corner_radius);
      path->arcTo(left, bottom, center_x, bottom, corner_radius);
      break;
    case views::BubbleBorder::BOTTOM_LEFT:
    case views::BubbleBorder::BOTTOM_RIGHT:
      path->moveTo(center_x, top);
      path->arcTo(left, top, left, center_y, corner_radius);
      path->arcTo(left, bottom, center_x  - half_arrow_width, bottom,
                  corner_radius);
      path->lineTo(center_x + arrow_offset - half_arrow_width, bottom);
      path->lineTo(center_x + arrow_offset, bottom + arrow_height);
      path->lineTo(center_x + arrow_offset + half_arrow_width, bottom);
      path->arcTo(right, bottom, right, center_y, corner_radius);
      path->arcTo(right, top, center_x, top, corner_radius);
      break;
    case views::BubbleBorder::LEFT_TOP:
    case views::BubbleBorder::LEFT_BOTTOM:
      path->moveTo(right, center_y);
      path->arcTo(right, top, center_x, top, corner_radius);
      path->arcTo(left, top, left, center_y + arrow_offset - half_arrow_width,
                  corner_radius);
      path->lineTo(left, center_y + arrow_offset - half_arrow_width);
      path->lineTo(left - arrow_height, center_y + arrow_offset);
      path->lineTo(left, center_y + arrow_offset + half_arrow_width);
      path->arcTo(left, bottom, center_x, bottom, corner_radius);
      path->arcTo(right, bottom, right, center_y, corner_radius);
      break;
    case views::BubbleBorder::RIGHT_TOP:
    case views::BubbleBorder::RIGHT_BOTTOM:
      path->moveTo(left, center_y);
      path->arcTo(left, bottom, center_x, bottom, corner_radius);
      path->arcTo(right, bottom,
                  right, center_y + arrow_offset + half_arrow_width,
                  corner_radius);
      path->lineTo(right, center_y + arrow_offset + half_arrow_width);
      path->lineTo(right + arrow_height, center_y + arrow_offset);
      path->lineTo(right, center_y + arrow_offset - half_arrow_width);
      path->arcTo(right, top, center_x, top, corner_radius);
      path->arcTo(left, top, left, center_y, corner_radius);
      break;
    default:
      // No arrows.
      path->addRoundRect(gfx::RectToSkRect(bounds),
                         corner_radius,
                         corner_radius);
      break;
  }

  path->close();
}

}  // namespace

namespace app_list {

AppListBubbleBorder::AppListBubbleBorder(views::View* app_list_view,
                                         views::View* search_box_view)
    : views::BubbleBorder(views::BubbleBorder::BOTTOM_RIGHT,
                          views::BubbleBorder::NO_SHADOW),
      app_list_view_(app_list_view),
      search_box_view_(search_box_view) {
  const gfx::ShadowValue kShadows[] = {
    // Offset (0, 5), blur=30, color=0.36 black
    gfx::ShadowValue(gfx::Point(0, 5), 30, SkColorSetARGB(0x72, 0, 0, 0)),
  };
  shadows_.assign(kShadows, kShadows + arraysize(kShadows));
}

AppListBubbleBorder::~AppListBubbleBorder() {
}

bool AppListBubbleBorder::ArrowAtTopOrBottom() const {
  return arrow_location() == views::BubbleBorder::TOP_LEFT ||
      arrow_location() == views::BubbleBorder::TOP_RIGHT ||
      arrow_location() == views::BubbleBorder::BOTTOM_LEFT ||
      arrow_location() == views::BubbleBorder::BOTTOM_RIGHT;
}

bool AppListBubbleBorder::ArrowOnLeftOrRight() const {
  return arrow_location() == views::BubbleBorder::LEFT_TOP ||
      arrow_location() == views::BubbleBorder::LEFT_BOTTOM ||
      arrow_location() == views::BubbleBorder::RIGHT_TOP ||
      arrow_location() == views::BubbleBorder::RIGHT_BOTTOM;
}

void AppListBubbleBorder::GetMask(const gfx::Rect& bounds,
                                  gfx::Path* mask) const {
  gfx::Insets insets;
  GetInsets(&insets);

  gfx::Rect content_bounds(bounds);
  content_bounds.Inset(insets);

  BuildShape(content_bounds,
             arrow_location(),
             SkIntToScalar(GetArrowOffset()),
             SkIntToScalar(kBorderSize),
             mask);
}

int AppListBubbleBorder::GetArrowOffset() const {
  if (ArrowAtTopOrBottom()) {
    // Picks x offset and moves bubble arrow in the opposite direction.
    // i.e. If bubble bounds is moved to right (positive offset), we need to
    // move arrow to left so that it points to the same position.
    return -offset_.x();
  } else if (ArrowOnLeftOrRight()) {
    // Picks y offset and moves bubble arrow in the opposite direction.
    return -offset_.y();
  }

  // Other style does not have an arrow, so return 0.
  return 0;
}

void AppListBubbleBorder::PaintBackground(gfx::Canvas* canvas,
                                          const gfx::Rect& bounds) const {
  const gfx::Rect search_box_view_bounds =
      app_list_view_->ConvertRectToWidget(search_box_view_->bounds());
  gfx::Rect search_box_rect(bounds.x(),
                            bounds.y(),
                            bounds.width(),
                            search_box_view_bounds.bottom() - bounds.y());

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kSearchBoxBackground);
  canvas->DrawRect(search_box_rect, paint);

  gfx::Rect seperator_rect(search_box_rect);
  seperator_rect.set_y(seperator_rect.bottom());
  seperator_rect.set_height(kTopSeparatorSize);
  canvas->FillRect(seperator_rect, kTopSeparatorColor);

  gfx::Rect contents_rect(bounds.x(),
                          seperator_rect.bottom(),
                          bounds.width(),
                          bounds.bottom() - seperator_rect.bottom());

  paint.setColor(kContentsBackground);
  canvas->DrawRect(contents_rect, paint);
}

void AppListBubbleBorder::GetInsets(gfx::Insets* insets) const {
  // Negate to change from outer margin to inner padding.
  gfx::Insets shadow_padding(-gfx::ShadowValue::GetMargin(shadows_));

  if (arrow_location() == views::BubbleBorder::TOP_LEFT ||
      arrow_location() == views::BubbleBorder::TOP_RIGHT) {
    // Arrow at top.
    insets->Set(shadow_padding.top() + kArrowHeight,
                shadow_padding.left(),
                shadow_padding.bottom(),
                shadow_padding.right());
  } else if (arrow_location() == views::BubbleBorder::BOTTOM_LEFT ||
      arrow_location() == views::BubbleBorder::BOTTOM_RIGHT) {
    // Arrow at bottom.
    insets->Set(shadow_padding.top(),
                shadow_padding.left(),
                shadow_padding.bottom() + kArrowHeight,
                shadow_padding.right());
  } else if (arrow_location() == views::BubbleBorder::LEFT_TOP ||
      arrow_location() == views::BubbleBorder::LEFT_BOTTOM) {
    // Arrow on left.
    insets->Set(shadow_padding.top(),
                shadow_padding.left() + kArrowHeight,
                shadow_padding.bottom(),
                shadow_padding.right());
  } else if (arrow_location() == views::BubbleBorder::RIGHT_TOP ||
      arrow_location() == views::BubbleBorder::RIGHT_BOTTOM) {
    // Arrow on right.
    insets->Set(shadow_padding.top(),
                shadow_padding.left(),
                shadow_padding.bottom(),
                shadow_padding.right() + kArrowHeight);
  }
}

gfx::Rect AppListBubbleBorder::GetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size) const {
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.width(), insets.height());

  // Negate to change from outer margin to inner padding.
  gfx::Insets shadow_padding(-gfx::ShadowValue::GetMargin(shadows_));

  // Anchor center that arrow aligns with.
  const int anchor_center_x =
      (position_relative_to.x() + position_relative_to.right()) / 2;
  const int anchor_center_y =
      (position_relative_to.y() + position_relative_to.bottom()) / 2;

  // Arrow position relative to top-left of bubble. |arrow_tip_x| is used for
  // arrow at the top or bottom and |arrow_tip_y| is used for arrow on left or
  // right. The 1px offset for |arrow_tip_y| is needed because the app list grid
  // icon start at a different position (1px earlier) compared with bottom
  // launcher bar.
  // TODO(xiyuan): Remove 1px offset when app list icon image asset is updated.
  int arrow_tip_x = insets.left() + contents_size.width() / 2 +
      GetArrowOffset();
  int arrow_tip_y = insets.top() + contents_size.height() / 2 +
      GetArrowOffset() + 1;

  if (arrow_location() == views::BubbleBorder::TOP_LEFT ||
      arrow_location() == views::BubbleBorder::TOP_RIGHT) {
    // Arrow at top.
    return gfx::Rect(
        gfx::Point(anchor_center_x - arrow_tip_x,
                   position_relative_to.bottom() - shadow_padding.top() -
                       kArrowHeight),
        border_size);
  } else if (arrow_location() == views::BubbleBorder::BOTTOM_LEFT ||
      arrow_location() == views::BubbleBorder::BOTTOM_RIGHT) {
    // Arrow at bottom.
    return gfx::Rect(
        gfx::Point(anchor_center_x - arrow_tip_x,
                   position_relative_to.y() - border_size.height() +
                       shadow_padding.bottom() + kArrowHeight),
        border_size);
  } else if (arrow_location() == views::BubbleBorder::LEFT_TOP ||
      arrow_location() == views::BubbleBorder::LEFT_BOTTOM) {
    // Arrow on left.
    return gfx::Rect(
        gfx::Point(position_relative_to.right() - shadow_padding.left() -
                       kArrowHeight,
                   anchor_center_y - arrow_tip_y),
        border_size);
  } else if (arrow_location() == views::BubbleBorder::RIGHT_TOP ||
      arrow_location() == views::BubbleBorder::RIGHT_BOTTOM) {
    // Arrow on right.
    return gfx::Rect(
        gfx::Point(position_relative_to.x() - border_size.width() +
                       shadow_padding.right() + kArrowHeight,
                   anchor_center_y - arrow_tip_y),
        border_size);
  }

  // No arrow bubble, center align with anchor.
  return position_relative_to.Center(border_size);
}

void AppListBubbleBorder::Paint(const views::View& view,
                                gfx::Canvas* canvas) const {
  gfx::Insets insets;
  GetInsets(&insets);

  gfx::Rect content_bounds = view.bounds();
  content_bounds.Inset(insets);

  SkPath path;
  // Pads with 0.5 pixel since anti alias is used.
  BuildShape(content_bounds,
             arrow_location(),
             SkIntToScalar(GetArrowOffset()),
             SkDoubleToScalar(0.5),
             &path);

  // Draw border and shadow. Note fill is needed to generate enough shadow.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStrokeAndFill_Style);
  paint.setStrokeWidth(SkIntToScalar(kBorderSize));
  paint.setColor(kBorderColor);
  SkSafeUnref(paint.setLooper(gfx::CreateShadowDrawLooper(shadows_)));
  canvas->DrawPath(path, paint);

  // Pads with kBoprderSize pixels to leave space for border lines.
  BuildShape(content_bounds,
             arrow_location(),
             SkIntToScalar(GetArrowOffset()),
             SkIntToScalar(kBorderSize),
             &path);
  canvas->Save();
  canvas->ClipPath(path);

  // Use full bounds so that arrow is also painted.
  const gfx::Rect& bounds = view.bounds();
  PaintBackground(canvas, bounds);

  canvas->Restore();
}

}  // namespace app_list
