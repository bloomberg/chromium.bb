// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/progress_bar_view.h"

#include <algorithm>

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/insets.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/painter.h"

namespace app_list {

namespace {

// Image assets for the bar part.
const int kProgressBarImages[] = {
  IDR_APP_LIST_ITEM_PROGRESS_LEFT,
  IDR_APP_LIST_ITEM_PROGRESS_CENTER,
  IDR_APP_LIST_ITEM_PROGRESS_RIGHT
};

}  // namespace

ProgressBarView::ProgressBarView()
    : background_painter_(views::Painter::CreateImagePainter(
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_APP_LIST_ITEM_PROGRESS_BACKGROUND),
          gfx::Insets(2, 2, 2, 2))),
      bar_painter_(new views::HorizontalPainter(kProgressBarImages)) {}

ProgressBarView::~ProgressBarView() {}

gfx::Size ProgressBarView::GetPreferredSize() const {
  return background_painter_->GetMinimumSize();
}

void ProgressBarView::OnPaint(gfx::Canvas* canvas) {
  gfx::Size paint_size = size();

  const gfx::Size min_size(background_painter_->GetMinimumSize());
  if (paint_size.width() < min_size.width() ||
      paint_size.height() < min_size.height()) {
    return;
  }

  background_painter_->Paint(canvas, paint_size);

  const int kBarEndWidth = 4;
  const int bar_width = paint_size.width() - kBarEndWidth;
  DCHECK_GE(bar_width, 0);

  // Map normalized value in [0,1] to the pixel width in
  // [kBarEndWidth, view_size.width()].
  paint_size.set_width(kBarEndWidth + bar_width * GetNormalizedValue());
  bar_painter_->Paint(canvas, paint_size);
}

}  // namespace app_list
