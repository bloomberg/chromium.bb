// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/test/test_app_list_presenter.h"

#include <memory>

#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ui/app_list/presenter/test/test_app_list_view_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"

namespace app_list {
namespace test {

TestAppListPresenter::TestAppListPresenter()
    : app_list_presenter_impl_(std::make_unique<
                               ash::AppListPresenterDelegateFactory>(
          std::make_unique<app_list::test::TestAppListViewDelegateFactory>())),
      binding_(this) {}

TestAppListPresenter::~TestAppListPresenter() {}

mojom::AppListPresenterPtr TestAppListPresenter::CreateInterfacePtrAndBind() {
  mojom::AppListPresenterPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestAppListPresenter::Show(int64_t display_id) {
  app_list_presenter_impl_.Show(display_id);
}

void TestAppListPresenter::Dismiss() {
  app_list_presenter_impl_.Dismiss();
}

void TestAppListPresenter::ToggleAppList(int64_t display_id) {
  app_list_presenter_impl_.ToggleAppList(display_id);
}

void TestAppListPresenter::StartVoiceInteractionSession() {
  voice_session_count_++;
}

void TestAppListPresenter::ToggleVoiceInteractionSession() {
  voice_session_count_++;
}

void TestAppListPresenter::UpdateYPositionAndOpacity(int y_position_in_screen,
                                                     float background_opacity) {
  app_list_presenter_impl_.UpdateYPositionAndOpacity(y_position_in_screen,
                                                     background_opacity);
}

void TestAppListPresenter::EndDragFromShelf(
    mojom::AppListState app_list_state) {
  app_list_presenter_impl_.EndDragFromShelf(app_list_state);
}

void TestAppListPresenter::ProcessMouseWheelOffset(int y_scroll_offset) {
  app_list_presenter_impl_.ProcessMouseWheelOffset(y_scroll_offset);
}

AppListViewState TestAppListPresenter::GetAppListViewState() {
  AppListView* view = app_list_presenter_impl_.GetView();

  return view ? view->app_list_state() : AppListViewState::CLOSED;
}

void TestAppListPresenter::FlushForTesting() {
  binding_.FlushForTesting();
}

}  // namespace test
}  // namespace app_list
