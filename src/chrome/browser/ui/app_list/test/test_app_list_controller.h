// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_

#include "ash/public/cpp/app_list/app_list_controller.h"

#include "base/observer_list.h"

namespace ash {
class AppListControllerObserver;
}

namespace test {

// A fake app list controller used by browser side unit tests to emulate the
// interaction between browser and ash in tests.
// Currently, it only tracks app list visibility updates using ShowAppList() and
// DismissAppList().
class TestAppListController : public ash::AppListController {
 public:
  TestAppListController();
  TestAppListController(const TestAppListController&) = delete;
  TestAppListController& operator=(const TestAppListController&) = delete;
  ~TestAppListController() override;

  // ash::AppListController:
  void SetClient(ash::AppListClient* client) override {}
  ash::AppListClient* GetClient() override;
  void AddObserver(ash::AppListControllerObserver* observer) override;
  void RemoveObserver(ash::AppListControllerObserver* obsever) override;
  void SetActiveModel(int profile_id,
                      ash::AppListModel* model,
                      ash::SearchModel* search_model) override {}
  void ClearActiveModel() override {}
  void NotifyProcessSyncChangesFinished() override {}
  void ShowAppList() override;
  void DismissAppList() override;
  void GetAppInfoDialogBounds(
      GetAppInfoDialogBoundsCallback callback) override {}
  aura::Window* GetWindow() override;
  bool IsVisible(const absl::optional<int64_t>& display_id) override;
  bool IsVisible() override;
  void UpdateAppListWithNewTemporarySortOrder(
      const absl::optional<ash::AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure) override;
  void HideContinueSection() override {}

 private:
  void NotifyAppListVisibilityChanged();

  // The visibility state set using (Show|Dismiss)AppList.
  bool visible_ = false;

  base::ObserverList<ash::AppListControllerObserver> observers_;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_H_
