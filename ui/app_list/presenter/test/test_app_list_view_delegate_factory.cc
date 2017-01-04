// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/presenter/test/test_app_list_view_delegate_factory.h"

namespace app_list {
namespace test {

TestAppListViewDelegateFactory::TestAppListViewDelegateFactory() {}

TestAppListViewDelegateFactory::~TestAppListViewDelegateFactory() {}

AppListViewDelegate* TestAppListViewDelegateFactory::GetDelegate() {
  return &delegate_;
}

}  // namespace test
}  // namespace app_list
