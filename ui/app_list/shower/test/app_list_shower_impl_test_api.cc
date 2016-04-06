// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/shower/test/app_list_shower_impl_test_api.h"

#include "ui/app_list/shower/app_list_shower_impl.h"
#include "ui/app_list/views/app_list_view.h"

namespace app_list {
namespace test {

AppListShowerImplTestApi::AppListShowerImplTestApi(AppListShowerImpl* shower)
    : shower_(shower) {}

AppListView* AppListShowerImplTestApi::view() {
  return shower_->view_;
}

AppListShowerDelegate* AppListShowerImplTestApi::shower_delegate() {
  return shower_->shower_delegate_.get();
}

}  // namespace test
}  // namespace app_list
