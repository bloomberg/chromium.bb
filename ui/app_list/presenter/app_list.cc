// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/app_list.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/presenter/app_list_delegate.h"
#include "ui/events/event.h"

namespace app_list {

AppList::AppList() {}

AppList::~AppList() {}

void AppList::BindRequest(mojom::AppListRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

mojom::AppListPresenter* AppList::GetAppListPresenter() {
  return presenter_.get();
}

void AppList::Show(int64_t display_id, AppListShowSource show_source) {
  if (presenter_) {
    UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                              show_source, app_list::kMaxAppListToggleMethod);
    presenter_->Show(display_id);
  }
}

void AppList::UpdateYPositionAndOpacity(int y_position_in_screen,
                                        float background_opacity) {
  if (presenter_) {
    presenter_->UpdateYPositionAndOpacity(y_position_in_screen,
                                          background_opacity);
  }
}

void AppList::EndDragFromShelf(mojom::AppListState app_list_state) {
  if (presenter_)
    presenter_->EndDragFromShelf(app_list_state);
}

void AppList::ProcessMouseWheelEvent(const ui::MouseWheelEvent& event) {
  if (presenter_)
    presenter_->ProcessMouseWheelOffset(event.offset().y());
}

void AppList::Dismiss() {
  if (presenter_)
    presenter_->Dismiss();
}

void AppList::ToggleAppList(int64_t display_id, AppListShowSource show_source) {
  if (presenter_) {
    if (!IsVisible()) {
      UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                                show_source, app_list::kMaxAppListToggleMethod);
    }
    presenter_->ToggleAppList(display_id);
  }
}

void AppList::StartVoiceInteractionSession() {
  if (presenter_)
    presenter_->StartVoiceInteractionSession();
}

void AppList::ToggleVoiceInteractionSession() {
  if (presenter_)
    presenter_->ToggleVoiceInteractionSession();
}

bool AppList::IsVisible() const {
  return visible_;
}

bool AppList::GetTargetVisibility() const {
  return target_visible_;
}

void AppList::SetAppListPresenter(mojom::AppListPresenterPtr presenter) {
  presenter_ = std::move(presenter);
}

void AppList::OnTargetVisibilityChanged(bool visible) {
  target_visible_ = visible;
}

void AppList::OnVisibilityChanged(bool visible, int64_t display_id) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (delegate_)
    delegate_->OnAppListVisibilityChanged(visible, display_id);
}

void AppList::FlushForTesting() {
  bindings_.FlushForTesting();
}

}  // namespace app_list
