// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/camera_roll_opt_in_view.h"
#include "base/gtest_prod_util.h"
#include "chromeos/components/phonehub/camera_roll_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace chromeos {
namespace phonehub {
class UserActionRecorder;
}  // namespace phonehub
}  // namespace chromeos

namespace ash {

// A view in Phone Hub bubble that allows user view and access recently taken
// photos and videos from a connected device.
// Contains the header and one or more lines of clickable thumbnails.
// This view will automatically hide if no Camera Roll items are available.
class ASH_EXPORT CameraRollView
    : public views::View,
      public chromeos::phonehub::CameraRollManager::Observer {
 public:
  CameraRollView(chromeos::phonehub::CameraRollManager* camera_roll_manager,
                 chromeos::phonehub::UserActionRecorder* user_action_recorder);
  ~CameraRollView() override;
  CameraRollView(CameraRollView&) = delete;
  CameraRollView operator=(CameraRollView&) = delete;

  // chromeos::phonehub::CameraRollManager::Observer:
  void OnCameraRollViewUiStateUpdated() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, DisplayOptInView);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, OptInAlready);
  FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ViewLayout);

  class CameraRollItemsView : public views::View {
   public:
    CameraRollItemsView();
    ~CameraRollItemsView() override;
    CameraRollItemsView(CameraRollItemsView&) = delete;
    CameraRollItemsView operator=(CameraRollItemsView&) = delete;

    void AddCameraRollItem(views::View* camera_roll_item);
    void Reset();

    // views::View:
    gfx::Size CalculatePreferredSize() const override;
    void Layout() override;
    const char* GetClassName() const override;

   private:
    FRIEND_TEST_ALL_PREFIXES(CameraRollViewTest, ViewLayout);

    gfx::Point GetCameraRollItemPosition(int index);
    void CalculateIdealBounds();

    views::ViewModelT<views::View> camera_roll_items_;
  };

  // Update the camera roll section to display the latest items.
  void Update();

  chromeos::phonehub::CameraRollManager* camera_roll_manager_ = nullptr;
  chromeos::phonehub::UserActionRecorder* user_action_recorder_ = nullptr;
  CameraRollOptInView* opt_in_view_ = nullptr;
  CameraRollItemsView* items_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_VIEW_H_
