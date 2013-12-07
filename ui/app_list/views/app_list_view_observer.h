// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_APP_LIST_VIEW_OBSERVER_H_
#define UI_APP_LIST_VIEWS_APP_LIST_VIEW_OBSERVER_H_

namespace views {
class Widget;
}

namespace app_list {

class AppListViewObserver {
 public:
  virtual void OnActivationChanged(views::Widget* widget, bool active) = 0;

 protected:
  virtual ~AppListViewObserver() {}
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_APP_LIST_VIEW_OBSERVER_H_
