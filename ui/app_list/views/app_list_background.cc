// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_background.h"

#include "base/command_line.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

namespace app_list {

AppListBackground::AppListBackground(int corner_radius)
    : corner_radius_(corner_radius) {
}

AppListBackground::~AppListBackground() {
}

void AppListBackground::Paint(gfx::Canvas* canvas,
                              views::View* view) const {
  gfx::Rect bounds = view->GetContentsBounds();

  if (corner_radius_ > 0) {
    canvas->Save();
    SkPath path;
    // Contents corner radius is 1px smaller than border corner radius.
    SkScalar radius = SkIntToScalar(corner_radius_ - 1);
    path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
    canvas->ClipPath(path, false);
  }

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);

  int contents_top = bounds.y();
  gfx::Rect contents_rect(bounds.x(),
                          contents_top,
                          bounds.width(),
                          bounds.bottom() - contents_top);

  paint.setColor(kContentsBackgroundColor);
  canvas->DrawRect(contents_rect, paint);

  if (corner_radius_ > 0)
    canvas->Restore();
}

}  // namespace app_list
