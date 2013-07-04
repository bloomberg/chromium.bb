// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_view_delegate.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace test {

AppListTestViewDelegate::AppListTestViewDelegate()
    : activate_count_(0),
      dismiss_count_(0),
      last_activated_(NULL),
      test_signin_delegate_(NULL) {
}

AppListTestViewDelegate::~AppListTestViewDelegate() {}

SigninDelegate* AppListTestViewDelegate::GetSigninDelegate() {
  return test_signin_delegate_;
}

void AppListTestViewDelegate::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
  callback.Run(base::FilePath());
}

void AppListTestViewDelegate::ActivateAppListItem(AppListItemModel* item,
                                                  int event_flags) {
  last_activated_ = item;
  ++activate_count_;
}

void AppListTestViewDelegate::Dismiss() {
  ++dismiss_count_;
}

gfx::ImageSkia AppListTestViewDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

base::string16 AppListTestViewDelegate::GetCurrentUserName() {
  return base::string16();
}

base::string16 AppListTestViewDelegate::GetCurrentUserEmail() {
  return base::string16();
}

}  // namespace test
}  // namespace app_list
