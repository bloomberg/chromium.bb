// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/test/test_app_list_presenter.h"

namespace app_list {
namespace test {

TestAppListPresenter::TestAppListPresenter() : binding_(this) {}

TestAppListPresenter::~TestAppListPresenter() {}

app_list::mojom::AppListPresenterPtr
TestAppListPresenter::CreateInterfacePtrAndBind() {
  return binding_.CreateInterfacePtrAndBind();
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

}  // namespace test
}  // namespace app_list
