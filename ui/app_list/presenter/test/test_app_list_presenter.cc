// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/test/test_app_list_presenter.h"

namespace app_list {
namespace test {

TestAppListPresenter::TestAppListPresenter() : binding_(this) {}

TestAppListPresenter::~TestAppListPresenter() {}

mojom::AppListPresenterPtr TestAppListPresenter::CreateInterfacePtrAndBind() {
  mojom::AppListPresenterPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestAppListPresenter::Show(int64_t display_id) {
  show_count_++;
}

void TestAppListPresenter::Dismiss() {
  dismiss_count_++;
}

void TestAppListPresenter::ToggleAppList(int64_t display_id) {
  toggle_count_++;
}

void TestAppListPresenter::StartVoiceInteractionSession() {
  voice_session_count_++;
}

void TestAppListPresenter::ToggleVoiceInteractionSession() {
  voice_session_count_++;
}

void TestAppListPresenter::UpdateYPositionAndOpacity(int y_position_in_screen,
                                                     float background_opacity) {
  set_y_position_count_++;
}

void TestAppListPresenter::EndDragFromShelf(
    mojom::AppListState app_list_state) {
  app_list_state_ = app_list_state;
}

void TestAppListPresenter::ProcessMouseWheelOffset(int y_scroll_offset) {
  process_mouse_wheel_offset_count_++;
}

}  // namespace test
}  // namespace app_list
