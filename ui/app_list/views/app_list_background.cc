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

// Size of top separator between searchbox and grid view.
const int kTopSeparatorSize = 1;

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
  views::View* search_box_view = main_view_->search_box_view();
  if (main_view_->visible() && search_box_view->visible()) {
    const gfx::Rect search_box_view_bounds =
        search_box_view->ConvertRectToWidget(search_box_view->GetLocalBounds());
    gfx::Rect search_box_rect(bounds.x(),
                              bounds.y(),
                              bounds.width(),
                              search_box_view_bounds.bottom() - bounds.y());

    paint.setColor(kSearchBoxBackground);
    canvas->DrawRect(search_box_rect, paint);

    gfx::Rect separator_rect(search_box_rect);
    separator_rect.set_y(separator_rect.bottom());
    separator_rect.set_height(kTopSeparatorSize);
    canvas->FillRect(separator_rect, kTopSeparatorColor);
    contents_top = separator_rect.bottom();
  }

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
      // Extra kContentsSwitcherSeparatorHeight pixels so the launcher page
      // indicator overlays the separator rect.
      separator_rect.set_y(contents_view_view_bounds.bottom() +
                           kContentsSwitcherSeparatorHeight);
      separator_rect.set_height(kBottomSeparatorSize);
      canvas->FillRect(separator_rect, kBottomSeparatorColor);
      int contents_switcher_top = separator_rect.bottom();
      gfx::Rect contents_switcher_rect(bounds.x(),
                                       contents_switcher_top,
                                       bounds.width(),
                                       bounds.bottom() - contents_switcher_top);
      paint.setColor(kContentsSwitcherBackgroundColor);
      canvas->DrawRect(contents_switcher_rect, paint);
    }
  }

  canvas->Restore();
}

}  // namespace app_list
