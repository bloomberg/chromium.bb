// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_APP_LIST_H_
#define UI_APP_LIST_PRESENTER_APP_LIST_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/app_list/presenter/app_list_presenter.mojom.h"
#include "ui/app_list/presenter/app_list_presenter_export.h"

namespace app_list {

class AppListDelegate;

// Stores the app list presenter interface pointer.
class APP_LIST_PRESENTER_EXPORT AppList : public mojom::AppList {
 public:
  AppList();
  ~AppList() override;

  // Binds the mojom::AppList interface request to this object.
  void BindRequest(mojom::AppListRequest request);

  // Get a raw pointer to the mojom::AppListPresenter interface; may be null.
  mojom::AppListPresenter* GetAppListPresenter();

  // Helper functions to call the underlying functionality on the presenter.
  void Show(int64_t display_id);
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity,
                                 bool is_end_gesture);

  void Dismiss();
  void ToggleAppList(int64_t display_id);
  void StartVoiceInteractionSession();
  void ToggleVoiceInteractionSession();

  // Helper functions to get the cached state as reported by the presenter.
  bool IsVisible() const;
  bool GetTargetVisibility() const;

  // mojom::AppList:
  void SetAppListPresenter(mojom::AppListPresenterPtr presenter) override;
  void OnTargetVisibilityChanged(bool visible) override;
  void OnVisibilityChanged(bool visible, int64_t display_id) override;

  void set_delegate(AppListDelegate* delegate) { delegate_ = delegate; }

 private:
  // Bindings for the mojom::AppList interface.
  mojo::BindingSet<mojom::AppList> bindings_;

  // App list presenter interface in chrome; used to show/hide the app list.
  mojom::AppListPresenterPtr presenter_;

  // The cached [target] visibility, as reported by the presenter.
  bool target_visible_ = false;
  bool visible_ = false;

  AppListDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppList);
};

}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_APP_LIST_H_
