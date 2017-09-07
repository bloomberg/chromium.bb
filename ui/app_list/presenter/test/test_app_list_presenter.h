// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_PRESENTER_H_
#define UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_PRESENTER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/app_list/presenter/app_list_presenter.mojom.h"

namespace app_list {
namespace test {

// A test implementation of AppListPresenter that records function call counts.
// Registers itself as the presenter for the app list on construction.
class TestAppListPresenter : public mojom::AppListPresenter {
 public:
  TestAppListPresenter();
  ~TestAppListPresenter() override;

  mojom::AppListPresenterPtr CreateInterfacePtrAndBind();

  // app_list::mojom::AppListPresenter:
  void Show(int64_t display_id) override;
  void Dismiss() override;
  void ToggleAppList(int64_t display_id) override;
  void StartVoiceInteractionSession() override;
  void ToggleVoiceInteractionSession() override;
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity) override;
  void EndDragFromShelf(mojom::AppListState app_list_state) override;
  void ProcessMouseWheelOffset(int y_scroll_offset) override;

  size_t show_count() const { return show_count_; }
  size_t dismiss_count() const { return dismiss_count_; }
  size_t toggle_count() const { return toggle_count_; }
  size_t voice_session_count() const { return voice_session_count_; }
  size_t voice_session_toggle_count() const {
    return voice_session_toggle_count_;
  }
  size_t set_y_position_count() const { return set_y_position_count_; }
  size_t process_mouse_wheel_offset_count() const {
    return process_mouse_wheel_offset_count_;
  }
  mojom::AppListState app_list_state() const { return app_list_state_; }

 private:
  size_t show_count_ = 0u;
  size_t dismiss_count_ = 0u;
  size_t toggle_count_ = 0u;
  size_t voice_session_count_ = 0u;
  size_t voice_session_toggle_count_ = 0u;
  size_t set_y_position_count_ = 0u;
  size_t process_mouse_wheel_offset_count_ = 0u;
  mojom::AppListState app_list_state_ = mojom::AppListState::CLOSED;

  mojo::Binding<mojom::AppListPresenter> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestAppListPresenter);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_PRESENTER_H_
