// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_H_
#define UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_H_

#include <stdint.h>

#include "ui/app_list/presenter/app_list_presenter_export.h"

namespace aura {
class Window;
}

namespace app_list {

// Interface for showing and hiding the app list.
class APP_LIST_PRESENTER_EXPORT AppListPresenter {
 public:
  virtual ~AppListPresenter() {}

  // Show/hide app list window. The |window| is used to deterime in
  // which display (in which the |window| exists) the app list should
  // be shown.
  virtual void Show(int64_t display_id) = 0;

  // Invoked to dismiss app list. This may leave the view open but hidden from
  // the user.
  virtual void Dismiss() = 0;

  // Show the app list if it is visible, hide it if it is hidden.
  virtual void ToggleAppList(int64_t display_id) = 0;

  // Returns target visibility. This may differ from IsVisible() if a
  // visibility transition is in progress.
  virtual bool GetTargetVisibility() const = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_H_
