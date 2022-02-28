// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/types/display_constants.h"

namespace app_list {
class SearchController;
}  // namespace app_list

class AppListClientWithProfileTest;
class AppListNotifierImpl;
class AppListModelUpdater;
class AppSyncUIStateWatcher;
class Profile;

class AppListClientImpl
    : public ash::AppListClient,
      public AppListControllerDelegate,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver,
      public TemplateURLServiceObserver {
 public:
  // Indicates the launcher usage state during the session started by a new user
  // (i.e. the session completing the OOBE flow) but before any account
  // switching. These are used in histograms, do not remove/renumber entries. If
  // you're adding to this enum with the intention that it will be logged,
  // update the AppListUsageStateByNewUsers enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AppListUsageStateByNewUsers {
    // Launcher is used during the session started by a new user.
    kUsed = 0,

    // Launcher is not used before destruction. The destruction can be triggered
    // in the following scenarios: logging out all account, shutting down the
    // device and system crashes.
    kNotUsedBeforeDestruction = 1,

    // Launcher is not used before switching accounts.
    kNotUsedBeforeSwitchingAccounts = 2,

    kMaxValue = kNotUsedBeforeSwitchingAccounts,
  };

  AppListClientImpl();
  AppListClientImpl(const AppListClientImpl&) = delete;
  AppListClientImpl& operator=(const AppListClientImpl&) = delete;
  ~AppListClientImpl() override;

  static AppListClientImpl* GetInstance();

  // ash::AppListClient:
  void OnAppListControllerDestroyed() override;
  void StartZeroStateSearch(base::OnceClosure on_done,
                            base::TimeDelta timeout) override;
  void StartSearch(const std::u16string& trimmed_query) override;
  void OpenSearchResult(int profile_id,
                        const std::string& result_id,
                        ash::AppListSearchResultType result_type,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                ash::SearchResultActionType action) override;
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewClosing() override;
  void ViewShown(int64_t display_id) override;
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags) override;
  void GetContextMenuModel(int profile_id,
                           const std::string& id,
                           bool add_sort_options,
                           GetContextMenuModelCallback callback) override;
  void OnAppListVisibilityWillChange(bool visible) override;
  void OnAppListVisibilityChanged(bool visible) override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visible) override;
  void OnQuickSettingsChanged(
      const std::string& setting_name,
      const std::map<std::string, int>& values) override;
  void NotifySearchResultsForLogging(
      const std::u16string& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int position_index) override;
  ash::AppListNotifier* GetNotifier() override;
  void LoadIcon(int profile_id, const std::string& app_id) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // AppListControllerDelegate overrides:
  void DismissView() override;
  aura::Window* GetAppListWindow() override;
  int64_t GetAppListDisplayId() override;
  bool IsAppPinned(const std::string& app_id) override;
  bool IsAppOpen(const std::string& app_id) const override;
  void PinApp(const std::string& app_id) override;
  void UnpinApp(const std::string& app_id) override;
  Pinnable GetPinnable(const std::string& app_id) override;
  void CreateNewWindow(bool incognito,
                       bool should_trigger_session_restore) override;
  void OpenURL(Profile* profile,
               const GURL& url,
               ui::PageTransition transition,
               WindowOpenDisposition disposition) override;

  // Associates this client with the current active user, called when this
  // client is accessed or active user is changed.
  void UpdateProfile();

  void ShowAppList();

  bool app_list_target_visibility() const {
    return app_list_target_visibility_;
  }
  bool app_list_visible() const { return app_list_visible_; }

  // Returns a pointer to control the app list views in ash.
  ash::AppListController* GetAppListController() const;

  AppListControllerDelegate* GetControllerDelegate();
  Profile* GetCurrentAppListProfile() const;

  app_list::SearchController* search_controller();

  void SetSearchControllerForTest(
      std::unique_ptr<app_list::SearchController> test_controller);

  AppListModelUpdater* GetModelUpdaterForTest();

  // Initializes as if a new user logged in for testing.
  void InitializeAsIfNewUserLoginForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(AppListClientWithProfileTest, CheckDataRace);

  struct StateForNewUser {
    // Indicates whether showing the app list has been recorded.
    bool showing_recorded = false;

    // Indicates whether any launcher action has been recorded.
    bool action_recorded = false;
  };

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Configures the AppList for the given |profile|.
  void SetProfile(Profile* profile);

  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  // Maybe records the metrics related to showing the app list.
  void MaybeRecordViewShown();

  // Records the browser window status + the opened search result type when
  // the result is opened from the search box.
  void RecordOpenedResultFromSearchBox(
      ash::AppListSearchResultType result_type);

  // Maybe records the launcher action. Launcher actions include activating an
  // app and opening a search result from either a suggestion chip or the search
  // box. `launched_from` indicates where the launcher action comes from.
  void MaybeRecordLauncherAction(ash::AppListLaunchedFrom launched_from);

  // The current display id showing the app list.
  int64_t display_id_ = display::kInvalidDisplayId;

  // Unowned pointer to the associated profile. May change if SetProfile is
  // called.
  Profile* profile_ = nullptr;

  // Unowned pointer to the model updater owned by AppListSyncableService. Will
  // change if |profile_| changes.
  AppListModelUpdater* current_model_updater_ = nullptr;

  // Store the mappings between profiles and AppListModelUpdater instances.
  // In multi-profile mode, mojo callings from the Ash process to access the app
  // list items should be dealt with the correct AppListModelUpdater instance.
  // Otherwise data race may happen, like the issue 939755
  // (https://crbug.com/939755).
  // TODO: Replace the mojo interface functions provided by AppListClient with
  // callbacks.
  std::map<int, AppListModelUpdater*> profile_model_mappings_;

  std::unique_ptr<app_list::SearchController> search_controller_;
  std::unique_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  ash::AppListController* app_list_controller_ = nullptr;

  std::unique_ptr<AppListNotifierImpl> app_list_notifier_;

  // Records the app list state for the session started by a new user. It
  // gets reset when:
  // (1) the active user changes, or
  // (2) the user signs out all accounts. `AppListClientImpl` is destructed in
  // this scenario.
  absl::optional<StateForNewUser> state_for_new_user_;

  // Indicates when the session of a new user becomes active. If there is no new
  // users logged in, `new_user_session_activation_time_` is null.
  absl::optional<base::Time> new_user_session_activation_time_;

  bool app_list_target_visibility_ = false;
  bool app_list_visible_ = false;

  base::WeakPtrFactory<AppListClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CLIENT_IMPL_H_
