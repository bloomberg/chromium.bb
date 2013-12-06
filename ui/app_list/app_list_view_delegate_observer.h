// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_

#include "ui/app_list/app_list_export.h"

namespace app_list {

class APP_LIST_EXPORT AppListViewDelegateObserver {
 public:
  // Invoked when the Profiles shown on the app list change, or the active
  // profile changes its signin status.
  virtual void OnProfilesChanged() = 0;

 protected:
  virtual ~AppListViewDelegateObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_
