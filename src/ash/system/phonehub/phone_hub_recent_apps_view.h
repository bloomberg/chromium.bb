// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APPS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APPS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/components/phonehub/recent_apps_interaction_handler.h"
#include "base/gtest_prod_util.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

// A view in Phone Hub bubble that allows user to relaunch a streamed app from
// the recent apps list.
class ASH_EXPORT PhoneHubRecentAppsView
    : public views::View,
      public phonehub::RecentAppsInteractionHandler::Observer {
 public:
  explicit PhoneHubRecentAppsView(
      phonehub::RecentAppsInteractionHandler* recent_apps_interaction_handler);
  ~PhoneHubRecentAppsView() override;
  PhoneHubRecentAppsView(PhoneHubRecentAppsView&) = delete;
  PhoneHubRecentAppsView operator=(PhoneHubRecentAppsView&) = delete;

  // phonehub::RecentAppsInteractionHandler::Observer:
  void OnRecentAppsUiStateUpdated() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest, TaskViewVisibility);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           SingleRecentAppButtonsView);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           MultipleRecentAppButtonsView);

  class PlaceholderView;

  class RecentAppButtonsView : public views::View {
   public:
    RecentAppButtonsView();
    ~RecentAppButtonsView() override;
    RecentAppButtonsView(RecentAppButtonsView&) = delete;
    RecentAppButtonsView operator=(RecentAppButtonsView&) = delete;

    // views::View:
    gfx::Size CalculatePreferredSize() const override;
    void Layout() override;
    const char* GetClassName() const override;

    views::View* AddRecentAppButton(
        std::unique_ptr<views::View> recent_app_button);
    void Reset();
  };

  // Update the view to reflect the most recently opened apps.
  void Update();

  RecentAppButtonsView* recent_app_buttons_view_ = nullptr;
  std::vector<views::View*> recent_app_button_list_;
  phonehub::RecentAppsInteractionHandler* recent_apps_interaction_handler_ =
      nullptr;
  PlaceholderView* placeholder_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APPS_VIEW_H_
