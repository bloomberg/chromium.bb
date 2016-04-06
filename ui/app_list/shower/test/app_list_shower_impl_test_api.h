// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SHOWER_TEST_APP_LIST_SHOWER_IMPL_TEST_API_H_
#define UI_APP_LIST_SHOWER_TEST_APP_LIST_SHOWER_IMPL_TEST_API_H_

#include "base/macros.h"

namespace app_list {
class AppListShowerDelegate;
class AppListShowerImpl;
class AppListView;

namespace test {

// Accesses private data from an AppListController for testing.
class AppListShowerImplTestApi {
 public:
  explicit AppListShowerImplTestApi(AppListShowerImpl* shower);

  AppListView* view();
  AppListShowerDelegate* shower_delegate();

 private:
  AppListShowerImpl* const shower_;

  DISALLOW_COPY_AND_ASSIGN(AppListShowerImplTestApi);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_SHOWER_TEST_APP_LIST_SHOWER_IMPL_TEST_API_H_
