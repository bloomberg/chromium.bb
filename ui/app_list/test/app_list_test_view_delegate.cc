// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_view_delegate.h"

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace test {

AppListTestViewDelegate::AppListTestViewDelegate()
    : dismiss_count_(0),
      test_signin_delegate_(NULL),
      model_(new AppListTestModel) {
}

AppListTestViewDelegate::~AppListTestViewDelegate() {}

bool AppListTestViewDelegate::ForceNativeDesktop() const {
  return false;
}

AppListModel* AppListTestViewDelegate::GetModel() {
  return model_.get();
}

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

const AppListViewDelegate::Users& AppListTestViewDelegate::GetUsers() const {
  return users_;
}

void AppListTestViewDelegate::ReplaceTestModel(int item_count) {
  model_.reset(new AppListTestModel);
  model_->PopulateApps(item_count);
}

}  // namespace test
}  // namespace app_list
