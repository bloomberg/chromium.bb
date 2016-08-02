// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_TEST_APP_LIST_VIEW_TEST_API_H_
#define UI_APP_LIST_VIEWS_TEST_APP_LIST_VIEW_TEST_API_H_

#include "base/callback.h"
#include "base/macros.h"

namespace app_list {

class AppListView;

namespace test {

// Allow access to private variables of the AppListView for testing.
class AppListViewTestApi {
 public:
  explicit AppListViewTestApi(app_list::AppListView* view);

  bool is_overlay_visible();
  void SetNextPaintCallback(const base::Closure& callback);

 private:
  app_list::AppListView* view_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewTestApi);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_TEST_APP_LIST_VIEW_TEST_API_H_
