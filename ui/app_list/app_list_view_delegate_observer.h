// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_

#include "ui/app_list/app_list_export.h"

namespace app_list {

class APP_LIST_EXPORT AppListViewDelegateObserver {
 public:
  // Invoked when wallpaper prominent colors are changed.
  virtual void OnWallpaperColorsChanged() = 0;

 protected:
  virtual ~AppListViewDelegateObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_
