// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/folder_image_source.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const int kItemIconDimension = 16;

}  // namespace

namespace app_list {

FolderImageSource::FolderImageSource(const Icons& icons, const gfx::Size& size)
    : gfx::CanvasImageSource(size, false), icons_(icons), size_(size) {
  DCHECK(icons.size() <= kNumFolderTopItems);
}

FolderImageSource::~FolderImageSource() {
}

// static
gfx::Size FolderImageSource::ItemIconSize() {
  return gfx::Size(kItemIconDimension, kItemIconDimension);
}

// static
std::vector<gfx::Rect> FolderImageSource::GetTopIconsBounds(
    const gfx::Rect& folder_icon_bounds) {
  const int delta_to_center = 1;
  gfx::Point icon_center = folder_icon_bounds.CenterPoint();
  std::vector<gfx::Rect> top_icon_bounds;

  // Get the top left icon bounds.
  int left_x = icon_center.x() - kItemIconDimension - delta_to_center;
  int top_y = icon_center.y() - kItemIconDimension - delta_to_center;
  gfx::Rect top_left(left_x, top_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(top_left);

  // Get the top right icon bounds.
  int right_x = icon_center.x() + delta_to_center;
  gfx::Rect top_right(right_x, top_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(top_right);

  // Get the bottom left icon bounds.
  int bottom_y = icon_center.y() + delta_to_center;
  gfx::Rect bottom_left(
      left_x, bottom_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(bottom_left);

  // Get the bottom right icon bounds.
  gfx::Rect bottom_right(
      right_x, bottom_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(bottom_right);

  return top_icon_bounds;
}

void FolderImageSource::DrawIcon(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& icon,
                                 const gfx::Size icon_size,
                                 int x,
                                 int y) {
  if (icon.isNull())
    return;

  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, icon_size));
  canvas->DrawImageInt(resized,
                       0,
                       0,
                       resized.width(),
                       resized.height(),
                       x,
                       y,
                       resized.width(),
                       resized.height(),
                       true);
}

void FolderImageSource::Draw(gfx::Canvas* canvas) {
  // Draw circle for folder shadow.
  gfx::PointF shadow_center(size().width() / 2, size().height() / 2);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setColor(kFolderShadowColor);
  canvas->sk_canvas()->drawCircle(
      shadow_center.x(), shadow_center.y(), kFolderShadowRadius, paint);
  // Draw circle for folder bubble.
  gfx::PointF bubble_center(shadow_center);
  bubble_center.Offset(0, -kFolderShadowOffsetY);
  paint.setColor(kFolderBubbleColor);
  canvas->sk_canvas()->drawCircle(
      bubble_center.x(), bubble_center.y(), kFolderBubbleRadius, paint);

  if (icons_.size() == 0)
    return;

  // Draw top items' icons.
  const gfx::Size item_icon_size(ItemIconSize());
  std::vector<gfx::Rect> top_icon_bounds = GetTopIconsBounds(gfx::Rect(size()));

  for (size_t i = 0; i < kNumFolderTopItems && i < icons_.size(); ++i) {
    DrawIcon(canvas,
             icons_[i],
             item_icon_size,
             top_icon_bounds[i].x(),
             top_icon_bounds[i].y());
  }
}

}  // namespace app_list
