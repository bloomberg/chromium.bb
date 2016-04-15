// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_TEST_APP_LIST_PRESENTER_IMPL_TEST_API_H_
#define UI_APP_LIST_PRESENTER_TEST_APP_LIST_PRESENTER_IMPL_TEST_API_H_

#include "base/macros.h"

namespace app_list {
class AppListPresenterDelegate;
class AppListPresenterImpl;
class AppListView;

namespace test {

// Accesses private data from an AppListController for testing.
class AppListPresenterImplTestApi {
 public:
  explicit AppListPresenterImplTestApi(AppListPresenterImpl* presenter);

  AppListView* view();
  AppListPresenterDelegate* presenter_delegate();

 private:
  AppListPresenterImpl* const presenter_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImplTestApi);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_TEST_APP_LIST_PRESENTER_IMPL_TEST_API_H_
