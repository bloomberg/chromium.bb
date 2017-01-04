// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_VIEW_DELEGATE_FACTORY_H_
#define UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_VIEW_DELEGATE_FACTORY_H_

#include "base/macros.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"

namespace app_list {
namespace test {

// A concrete AppListViewDelegateFactory for unit tests.
class TestAppListViewDelegateFactory : public AppListViewDelegateFactory {
 public:
  TestAppListViewDelegateFactory();
  ~TestAppListViewDelegateFactory() override;

  AppListViewDelegate* GetDelegate() override;

 private:
  AppListTestViewDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestAppListViewDelegateFactory);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_TEST_TEST_APP_LIST_VIEW_DELEGATE_FACTORY_H_
