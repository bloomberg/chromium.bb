// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_view_delegate.h"

namespace app_list {

AppListViewDelegate::User::User() : active(false) {
}

AppListViewDelegate::User::User(const User& other) = default;

AppListViewDelegate::User::~User() {}

#if !defined(OS_CHROMEOS)
base::string16 AppListViewDelegate::GetMessageTitle() const {
  return base::string16();
}
base::string16 AppListViewDelegate::GetMessageText(
    size_t* message_break) const {
  *message_break = 0;
  return base::string16();
}
base::string16 AppListViewDelegate::GetAppsShortcutName() const {
  return base::string16();
}
base::string16 AppListViewDelegate::GetLearnMoreText() const {
  return base::string16();
}
base::string16 AppListViewDelegate::GetLearnMoreLink() const {
  return base::string16();
}
gfx::ImageSkia* AppListViewDelegate::GetAppsIcon() const {
  return nullptr;
}
void AppListViewDelegate::OpenLearnMoreLink() {}
#endif

}  // namespace app_list
