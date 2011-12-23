// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/test/test_shell_delegate.h"

namespace aura_shell {
namespace test {

TestShellDelegate::TestShellDelegate() {
}

TestShellDelegate::~TestShellDelegate() {
}

void TestShellDelegate::CreateNewWindow() {
}

views::Widget* TestShellDelegate::CreateStatusArea() {
  return NULL;
}

void TestShellDelegate::RequestAppListWidget(
    const gfx::Rect& bounds,
    const SetWidgetCallback& callback) {
}

void TestShellDelegate::BuildAppListModel(AppListModel* model) {
}

AppListViewDelegate* TestShellDelegate::CreateAppListViewDelegate() {
  return NULL;
}

void TestShellDelegate::LauncherItemClicked(const LauncherItem& item) {
}

bool TestShellDelegate::ConfigureLauncherItem(LauncherItem* item) {
  return true;
}

}  // namespace test
}  // namespace aura_shell
