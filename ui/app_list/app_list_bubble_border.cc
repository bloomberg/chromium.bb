// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_bubble_border.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"

namespace {

const SkColor kSearchBoxBackground = SK_ColorWHITE;

// Colors and sizes of top separator between searchbox and grid view.
const SkColor kTopSeparatorColor = SkColorSetRGB(0xE5, 0xE5, 0xE5);
const int kTopSeparatorSize = 1;

}  // namespace

namespace app_list {

AppListBubbleBorder::AppListBubbleBorder(views::View* app_list_view,
                                         views::View* search_box_view)
    : views::BubbleBorder2(views::BubbleBorder::BOTTOM_RIGHT),
      app_list_view_(app_list_view),
      search_box_view_(search_box_view) {
}

AppListBubbleBorder::~AppListBubbleBorder() {
}

void AppListBubbleBorder::PaintBackground(gfx::Canvas* canvas,
                                          const gfx::Rect& bounds) const {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);

// TODO(benwells): Get these details painting on Windows.
#if !defined(OS_WIN)
  const gfx::Rect search_box_view_bounds =
      app_list_view_->ConvertRectToWidget(search_box_view_->bounds());
  gfx::Rect search_box_rect(bounds.x(),
                            bounds.y(),
                            bounds.width(),
                            search_box_view_bounds.bottom() - bounds.y());

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
#else
  gfx::Rect contents_rect(bounds);
#endif

  paint.setColor(kContentsBackgroundColor);
  canvas->DrawRect(contents_rect, paint);
}

}  // namespace app_list
