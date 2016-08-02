// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/test/app_list_view_test_api.h"

#include "ui/app_list/views/app_list_view.h"

namespace app_list {
namespace test {

AppListViewTestApi::AppListViewTestApi(app_list::AppListView* view)
    : view_(view) {}

bool AppListViewTestApi::is_overlay_visible() {
  DCHECK(view_->overlay_view_);
  return view_->overlay_view_->visible();
}

void AppListViewTestApi::SetNextPaintCallback(const base::Closure& callback) {
  view_->next_paint_callback_ = callback;
}

}  // namespace test
}  // namespace app_list
