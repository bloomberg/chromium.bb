// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_page.h"

#include "ash/app_list/views/contents_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

AppListPage::AppListPage() : contents_view_(nullptr) {}

AppListPage::~AppListPage() {}

void AppListPage::OnShown() {}

void AppListPage::OnWillBeShown() {}

void AppListPage::OnHidden() {}

void AppListPage::OnWillBeHidden() {}

void AppListPage::OnAnimationUpdated(double progress,
                                     AppListState from_state,
                                     AppListState to_state) {}

gfx::Size AppListPage::GetPreferredSearchBoxSize() const {
  return gfx::Size();
}

base::Optional<int> AppListPage::GetSearchBoxTop(
    AppListViewState view_state) const {
  return base::nullopt;
}

void AppListPage::UpdatePageBoundsForState(AppListState state,
                                           const gfx::Rect& contents_bounds,
                                           const gfx::Rect& search_box_bounds) {
  SetBoundsRect(
      GetPageBoundsForState(state, contents_bounds, search_box_bounds));
}

views::View* AppListPage::GetSelectedView() const {
  return nullptr;
}

views::View* AppListPage::GetFirstFocusableView() {
  return nullptr;
}

views::View* AppListPage::GetLastFocusableView() {
  return nullptr;
}

void AppListPage::AnimateOpacity(float current_progress,
                                 AppListViewState target_view_state,
                                 const OpacityAnimator& animator) {
  animator.Run(this, target_view_state != AppListViewState::kClosed);
}

void AppListPage::AnimateYPosition(AppListViewState target_view_state,
                                   const TransformAnimator& animator,
                                   float default_offset) {
  animator.Run(default_offset, layer(), this);
}

gfx::Rect AppListPage::GetAboveContentsOffscreenBounds(
    const gfx::Size& size) const {
  gfx::Rect rect(size);
  rect.set_y(-rect.height());
  return rect;
}

gfx::Rect AppListPage::GetBelowContentsOffscreenBounds(
    const gfx::Size& size) const {
  DCHECK(contents_view_);
  gfx::Rect rect(size);
  rect.set_y(contents_view_->GetContentsBounds().height());
  return rect;
}

gfx::Rect AppListPage::GetFullContentsBounds() const {
  DCHECK(contents_view_);
  return contents_view_->GetContentsBounds();
}

gfx::Rect AppListPage::GetDefaultContentsBounds() const {
  DCHECK(contents_view_);
  return contents_view_->GetContentsBounds();
}

const char* AppListPage::GetClassName() const {
  return "AppListPage";
}

}  // namespace ash
