// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_
#define UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_

#include "ui/app_list/app_list_export.h"

namespace app_list {

// TODO(mgiuca): Remove this class; it isn't used any more.
class APP_LIST_EXPORT AppListViewDelegateObserver {
 public:
  // Invoked on Chrome shutdown. This is only needed on Mac, since reference-
  // counting in Objective-C means that simply closing the window isn't enough
  // to guarantee references to Chrome objects are gone.
  virtual void OnShutdown() = 0;

 protected:
  virtual ~AppListViewDelegateObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_DELEGATE_OBSERVER_H_
