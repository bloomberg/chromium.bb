// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_background.h"

#include "base/command_line.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

namespace {

// Size of bottom separator between contents view and contents switcher.
const int kBottomSeparatorSize = 1;

}  // namespace

namespace app_list {

AppListBackground::AppListBackground(int corner_radius,
                                     AppListMainView* main_view)
    : corner_radius_(corner_radius),
      main_view_(main_view) {
}

AppListBackground::~AppListBackground() {
}

void AppListBackground::Paint(gfx::Canvas* canvas,
                              views::View* view) const {
  gfx::Rect bounds = view->GetContentsBounds();

  canvas->Save();
  SkPath path;
  // Contents corner radius is 1px smaller than border corner radius.
  SkScalar radius = SkIntToScalar(corner_radius_ - 1);
  path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
  canvas->ClipPath(path, false);

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);

  int contents_top = bounds.y();
  gfx::Rect contents_rect(bounds.x(),
                          contents_top,
                          bounds.width(),
                          bounds.bottom() - contents_top);

  paint.setColor(kContentsBackgroundColor);
  canvas->DrawRect(contents_rect, paint);

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    if (main_view_->visible()) {
      views::View* contents_view = main_view_->contents_view();
      const gfx::Rect contents_view_view_bounds =
          contents_view->ConvertRectToWidget(contents_view->GetLocalBounds());
      gfx::Rect separator_rect(contents_rect);
      separator_rect.Inset(
          kExperimentalWindowPadding + main_view_->GetInsets().left(), 0);
      separator_rect.set_y(contents_view_view_bounds.bottom() - 1);
      separator_rect.set_height(kBottomSeparatorSize);
      canvas->FillRect(separator_rect, kBottomSeparatorColor);
    }
  }

  canvas->Restore();
}

}  // namespace app_list
