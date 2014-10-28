// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_
#define UI_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_

#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace app_list {

class AppListFolderView;

// Draws the ink bubble indicating the boundary of the folder when user drags an
// item inside a folder.
class FolderBackgroundView : public views::View,
                             public ui::ImplicitAnimationObserver {
 public:
  enum ShowState {
    NO_BUBBLE,
    SHOW_BUBBLE,
    HIDE_BUBBLE,
  };

  FolderBackgroundView();
  ~FolderBackgroundView() override;

  // Updates the ink bubble's ShowState.
  void UpdateFolderContainerBubble(ShowState state);

  // Returns the radius of the folder container ink bubble.
  int GetFolderContainerBubbleRadius() const;

  void set_folder_view(AppListFolderView* folder_view) {
    folder_view_ = folder_view;
  }

 private:
  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  AppListFolderView* folder_view_;
  ShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(FolderBackgroundView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_FOLDER_BACKGROUND_VIEW_H_
