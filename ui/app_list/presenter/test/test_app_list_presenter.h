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
class TestAppListPresenter : public app_list::mojom::AppListPresenter {
 public:
  TestAppListPresenter();
  ~TestAppListPresenter() override;

  app_list::mojom::AppListPresenterPtr CreateInterfacePtrAndBind();

  // app_list::mojom::AppListPresenter:
  void Show(int64_t display_id) override;
  void Dismiss() override;
  void ToggleAppList(int64_t display_id) override;

  size_t show_count() const { return show_count_; }
  size_t dismiss_count() const { return dismiss_count_; }
  size_t toggle_count() const { return toggle_count_; }

 private:
  size_t show_count_ = 0u;
  size_t dismiss_count_ = 0u;
  size_t toggle_count_ = 0u;

  mojo::Binding<app_list::mojom::AppListPresenter> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestAppListPresenter);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_PRESENTER_H_
