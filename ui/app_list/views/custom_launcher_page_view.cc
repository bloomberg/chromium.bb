// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/custom_launcher_page_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

CustomLauncherPageView::CustomLauncherPageView(
    View* custom_launcher_page_contents)
    : custom_launcher_page_contents_(custom_launcher_page_contents) {
  SetLayoutManager(new views::FillLayout());
  AddChildView(custom_launcher_page_contents_);
}

CustomLauncherPageView::~CustomLauncherPageView() {
}

gfx::Rect CustomLauncherPageView::GetCollapsedLauncherPageBounds() const {
  gfx::Rect rect = GetFullContentsBounds();
  int page_height = rect.height();
  rect.set_y(page_height - kCustomPageCollapsedHeight);
  return rect;
}

gfx::Rect CustomLauncherPageView::GetPageBoundsForState(
    AppListModel::State state) const {
  gfx::Rect onscreen_bounds = GetFullContentsBounds();
  switch (state) {
    case AppListModel::STATE_CUSTOM_LAUNCHER_PAGE:
      return onscreen_bounds;
    case AppListModel::STATE_START:
      return GetCollapsedLauncherPageBounds();
    default:
      return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
  }
}

void CustomLauncherPageView::OnShown() {
  custom_launcher_page_contents_->SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CustomLauncherPageView::OnWillBeHidden() {
  custom_launcher_page_contents_->SetFocusBehavior(FocusBehavior::NEVER);
}

}  // namespace app_list
