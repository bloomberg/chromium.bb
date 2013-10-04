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
    : dismiss_count_(0),
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

void AppListTestViewDelegate::Dismiss() {
  ++dismiss_count_;
}

gfx::ImageSkia AppListTestViewDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

content::WebContents* AppListTestViewDelegate::GetStartPageContents() {
  return NULL;
}

}  // namespace test
}  // namespace app_list
