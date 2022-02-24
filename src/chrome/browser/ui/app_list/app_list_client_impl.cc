// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_client_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"
#include "chrome/browser/ui/app_list/app_list_notifier_impl_old.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state_watcher.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "components/session_manager/core/session_manager.h"
#include "extensions/common/extension.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace {

AppListClientImpl* g_app_list_client_instance = nullptr;

// Parameters used by the time duration metrics.
constexpr base::TimeDelta kTimeMetricsMin = base::Seconds(1);
constexpr base::TimeDelta kTimeMetricsMax = base::Days(7);
constexpr int kTimeMetricsBucketCount = 100;

bool IsTabletMode() {
  return ash::TabletMode::IsInTabletMode();
}

// Returns whether the session is active.
bool IsSessionActive() {
  return session_manager::SessionManager::Get()->session_state() ==
         session_manager::SessionState::ACTIVE;
}

bool CanBeHandledAsSystemUrl(const GURL& sanitized_url,
                             ui::PageTransition transition) {
  if (!PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
      !PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED)) {
    return false;
  }
  return ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(
      sanitized_url);
}

// IDs passed to ActivateItem are always of the form "<app id>". But app search
// results can have IDs either like "<app id>" or "chrome-extension://<app
// id>/". Since we cannot tell from the ID alone which is correct, try both and
// return a result if either succeeds.
ChromeSearchResult* FindAppResultByAppId(
    app_list::SearchController* search_controller,
    const std::string& app_id) {
  auto* result = search_controller->FindSearchResult(app_id);
  if (!result) {
    // Convert <app id> to chrome-extension://<app id>.
    result = search_controller->FindSearchResult(
        base::StrCat({extensions::kExtensionScheme, "://", app_id, "/"}));
  }
  return result;
}

}  // namespace

AppListClientImpl::AppListClientImpl()
    : app_list_controller_(ash::AppListController::Get()) {
  app_list_controller_->SetClient(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);

  DCHECK(!g_app_list_client_instance);
  g_app_list_client_instance = this;

  if (ash::features::IsProductivityLauncherEnabled()) {
    app_list_notifier_ =
        std::make_unique<AppListNotifierImpl>(app_list_controller_);
  } else {
    app_list_notifier_ =
        std::make_unique<AppListNotifierImplOld>(app_list_controller_);
  }
}

AppListClientImpl::~AppListClientImpl() {
  SetProfile(nullptr);

  auto* user_manager = user_manager::UserManager::Get();
  user_manager->RemoveSessionStateObserver(this);

  // We assume that the current user is new if `state_for_new_user_` has value.
  if (state_for_new_user_.has_value() &&
      !state_for_new_user_->showing_recorded) {
    DCHECK(user_manager->IsCurrentUserNew());

    // Prefer the function to the macro because the usage data is recorded no
    // more than once per second.
    if (IsTabletMode()) {
      base::UmaHistogramEnumeration(
          "Apps.AppListUsageByNewUsers.TabletMode",
          AppListUsageStateByNewUsers::kNotUsedBeforeDestruction);
    } else {
      base::UmaHistogramEnumeration(
          "Apps.AppListUsageByNewUsers.ClamshellMode",
          AppListUsageStateByNewUsers::kNotUsedBeforeDestruction);
    }
  }

  session_manager::SessionManager::Get()->RemoveObserver(this);

  DCHECK_EQ(this, g_app_list_client_instance);
  g_app_list_client_instance = nullptr;

  if (app_list_controller_)
    app_list_controller_->SetClient(nullptr);
}

// static
AppListClientImpl* AppListClientImpl::GetInstance() {
  return g_app_list_client_instance;
}

void AppListClientImpl::OnAppListControllerDestroyed() {
  // |app_list_controller_| could be released earlier, e.g. starting a kiosk
  // next session.
  app_list_controller_ = nullptr;
  if (current_model_updater_)
    current_model_updater_->SetActive(false);
}

void AppListClientImpl::StartSearch(const std::u16string& trimmed_query) {
  // TODO(crbug.com/1269115): In the productivity launcher we handle empty
  // queries, eg. from a user deleting a query, by re-routing them to
  // StartZeroStateSearch. We may want to change this behavior so that ash calls
  // StartZeroStateSearch directly.
  if (search_controller_) {
    if (trimmed_query.empty() &&
        ash::features::IsProductivityLauncherEnabled()) {
      // We use a long timeout here because the we don't have an
      // animation-related deadline for these results, unlike a call to
      // StartZeroStateSearch.
      StartZeroStateSearch(base::DoNothing(), base::Seconds(1));
    } else {
      search_controller_->StartSearch(trimmed_query);
    }
    OnSearchStarted();

    if (state_for_new_user_) {
      if (!state_for_new_user_->first_search_result_recorded &&
          state_for_new_user_->started_search && trimmed_query.empty()) {
        state_for_new_user_->first_search_result_recorded = true;
        RecordFirstSearchResult(ash::NO_RESULT, IsTabletMode());
      } else if (!trimmed_query.empty()) {
        state_for_new_user_->started_search = true;
      }
    }
  }

  app_list_notifier_->NotifySearchQueryChanged(trimmed_query);
}

void AppListClientImpl::StartZeroStateSearch(base::OnceClosure on_done,
                                             base::TimeDelta timeout) {
  if (search_controller_) {
    search_controller_->StartZeroState(std::move(on_done), timeout);
    OnSearchStarted();
  } else {
    std::move(on_done).Run();
  }
}

void AppListClientImpl::OpenSearchResult(int profile_id,
                                         const std::string& result_id,
                                         int event_flags,
                                         ash::AppListLaunchedFrom launched_from,
                                         ash::AppListLaunchType launch_type,
                                         int suggestion_index,
                                         bool launch_as_default) {
  if (!search_controller_)
    return;

  auto requested_model_updater_iter = profile_model_mappings_.find(profile_id);
  DCHECK(requested_model_updater_iter != profile_model_mappings_.end());
  DCHECK_EQ(current_model_updater_, requested_model_updater_iter->second);

  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result)
    return;

  app_list::LaunchData launch_data;
  launch_data.id = result_id;
  launch_data.result_type = result->result_type();
  launch_data.ranking_item_type =
      app_list::RankingItemTypeFromSearchResult(*result);
  launch_data.launch_type = launch_type;
  launch_data.launched_from = launched_from;
  launch_data.suggestion_index = suggestion_index;
  launch_data.score = result->relevance();

  if (launch_type == ash::AppListLaunchType::kAppSearchResult &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox &&
      launch_data.ranking_item_type == app_list::RankingItemType::kApp &&
      search_controller_->GetLastQueryLength() != 0) {
    ash::RecordSuccessfulAppLaunchUsingSearch(
        launched_from, search_controller_->GetLastQueryLength());
  }

  // Send training signal to search controller.
  search_controller_->Train(std::move(launch_data));

  app_list_notifier_->NotifyLaunched(
      result->display_type(),
      ash::AppListNotifier::Result(result_id, result->metrics_type()));

  RecordSearchResultOpenTypeHistogram(launched_from, result->metrics_type(),
                                      IsTabletMode());

  if (launch_as_default)
    RecordDefaultSearchResultOpenTypeHistogram(result->metrics_type());

  if (!search_controller_->GetLastQueryLength() &&
      launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox)
    RecordZeroStateSuggestionOpenTypeHistogram(result->metrics_type());

  if (launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox)
    RecordOpenedResultFromSearchBox(result->metrics_type());

  MaybeRecordLauncherAction(launched_from);

  if (state_for_new_user_ && state_for_new_user_->started_search &&
      !state_for_new_user_->first_search_result_recorded) {
    state_for_new_user_->first_search_result_recorded = true;
    RecordFirstSearchResult(result->metrics_type(), IsTabletMode());
  }

  // OpenResult may cause |result| to be deleted.
  search_controller_->OpenResult(result, event_flags);
}

void AppListClientImpl::InvokeSearchResultAction(
    const std::string& result_id,
    ash::SearchResultActionType action) {
  if (!search_controller_)
    return;
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (result)
    search_controller_->InvokeResultAction(result, action);
}

void AppListClientImpl::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  if (!search_controller_) {
    std::move(callback).Run(nullptr);
    return;
  }
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result) {
    std::move(callback).Run(nullptr);
    return;
  }
  result->GetContextMenuModel(base::BindOnce(
      [](GetContextMenuModelCallback callback,
         std::unique_ptr<ui::SimpleMenuModel> menu_model) {
        std::move(callback).Run(std::move(menu_model));
      },
      std::move(callback)));
}

void AppListClientImpl::ViewClosing() {
  display_id_ = display::kInvalidDisplayId;
  if (search_controller_)
    search_controller_->ViewClosing();
}

void AppListClientImpl::ViewShown(int64_t display_id) {
  if (current_model_updater_) {
    base::RecordAction(base::UserMetricsAction("Launcher_Show"));
    base::UmaHistogramSparse("Apps.AppListBadgedAppsCount",
                             current_model_updater_->BadgedItemCount());
  }
  display_id_ = display_id;
}

void AppListClientImpl::ActivateItem(int profile_id,
                                     const std::string& id,
                                     int event_flags,
                                     ash::AppListLaunchedFrom launched_from) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];

  // Pointless to notify the AppListModelUpdater of the activated item if the
  // |requested_model_updater| is not the current one, which means that the
  // active profile is changed. The same rule applies to the GetContextMenuModel
  // and ContextMenuItemSelected.
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }

  if (launched_from == ash::AppListLaunchedFrom::kLaunchedFromRecentApps) {
    auto* result = FindAppResultByAppId(search_controller_.get(), id);
    if (result) {
      app_list_notifier_->NotifyLaunched(
          result->display_type(),
          ash::AppListNotifier::Result(result->id(), result->metrics_type()));
    }
  }

  // TODO(crbug.com/1258415): All fields here except the ID are only relevant
  // to the old launcher, and can be cleaned up.
  // Send a training signal to the search controller.
  const auto* item = current_model_updater_->FindItem(id);
  if (item) {
    app_list::LaunchData launch_data;
    launch_data.id = id;
    // We don't have easy access to the search result type here, so
    // launch_data.result_type isn't set. However we have no need to distinguish
    // the type of apps launched from the grid in SearchController::Train.
    launch_data.ranking_item_type =
        app_list::RankingItemTypeFromChromeAppListItem(*item);
    launch_data.launched_from = ash::AppListLaunchedFrom::kLaunchedFromGrid;
    search_controller_->Train(std::move(launch_data));
  }

  MaybeRecordLauncherAction(ash::AppListLaunchedFrom::kLaunchedFromGrid);
  requested_model_updater->ActivateChromeItem(id, event_flags);
}

void AppListClientImpl::GetContextMenuModel(
    int profile_id,
    const std::string& id,
    bool add_sort_options,
    GetContextMenuModelCallback callback) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    std::move(callback).Run(nullptr);
    return;
  }
  requested_model_updater->GetContextMenuModel(
      id, add_sort_options,
      base::BindOnce(
          [](GetContextMenuModelCallback callback,
             std::unique_ptr<ui::SimpleMenuModel> menu_model) {
            std::move(callback).Run(std::move(menu_model));
          },
          std::move(callback)));
}

void AppListClientImpl::OnAppListVisibilityWillChange(bool visible) {
  app_list_target_visibility_ = visible;
  // TODO(crbug.com/1258415): This is only used in the old launcher, and can be
  // removed once the productivity launcher is launched.
  if (visible && search_controller_ &&
      !ash::features::IsProductivityLauncherEnabled()) {
    search_controller_->StartSearch(std::u16string());
  }
}

void AppListClientImpl::OnAppListVisibilityChanged(bool visible) {
  app_list_visible_ = visible;
  if (visible) {
    if (search_controller_)
      search_controller_->AppListShown();
    MaybeRecordViewShown();
  } else if (current_model_updater_) {
    current_model_updater_->OnAppListHidden();

    // Record whether user took action first time they opened the launcher.
    // Note that this is recorded only on first user session (otherwise
    // `state_for_new_user_` will not be set).
    if (state_for_new_user_ && state_for_new_user_->showing_recorded &&
        !state_for_new_user_->first_open_success_recorded) {
      state_for_new_user_->first_open_success_recorded = true;

      if (state_for_new_user_->shown_in_tablet_mode) {
        base::UmaHistogramBoolean(
            "Apps.AppList.SuccessfulFirstUsageByNewUsers.TabletMode",
            state_for_new_user_->action_recorded);
      } else {
        base::UmaHistogramBoolean(
            "Apps.AppList.SuccessfulFirstUsageByNewUsers.ClamshellMode",
            state_for_new_user_->action_recorded);
      }
    }
    // If the user started search, record no action if a result open event has
    // not been yet recorded.
    if (state_for_new_user_ && state_for_new_user_->started_search &&
        !state_for_new_user_->first_search_result_recorded) {
      state_for_new_user_->first_search_result_recorded = true;
      RecordFirstSearchResult(ash::NO_RESULT, IsTabletMode());
    }
  }
}

void AppListClientImpl::OnSearchResultVisibilityChanged(const std::string& id,
                                                        bool visibility) {
  if (!search_controller_)
    return;

  ChromeSearchResult* result = search_controller_->FindSearchResult(id);
  if (result == nullptr) {
    return;
  }
  result->OnVisibilityChanged(visibility);
}

void AppListClientImpl::OnQuickSettingsChanged(
    const std::string& setting_name,
    const std::map<std::string, int>& values) {
  // CrOS action recorder.
  app_list::CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat({"SettingsChanged-", setting_name})}, values);
}

void AppListClientImpl::ActiveUserChanged(user_manager::User* active_user) {
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    // In tests, the user before switching and the one after switching may
    // be both new. It should not happen in the real world.
    state_for_new_user_ = StateForNewUser();
  } else if (state_for_new_user_) {
    if (!state_for_new_user_->showing_recorded) {
      // We assume that the previous user before switching was new if
      // `state_for_new_user_` is not null.
      if (IsTabletMode()) {
        base::UmaHistogramEnumeration(
            "Apps.AppListUsageByNewUsers.TabletMode",
            AppListUsageStateByNewUsers::kNotUsedBeforeSwitchingAccounts);
      } else {
        base::UmaHistogramEnumeration(
            "Apps.AppListUsageByNewUsers.ClamshellMode",
            AppListUsageStateByNewUsers::kNotUsedBeforeSwitchingAccounts);
      }
    }
    state_for_new_user_.reset();
  }

  if (!active_user->is_profile_created())
    return;

  UpdateProfile();
}

void AppListClientImpl::UpdateProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  // AppListSyncableService is null in tests.
  if (syncable_service)
    SetProfile(profile);
}

void AppListClientImpl::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile)
    return;

  if (profile_) {
    DCHECK(current_model_updater_);
    current_model_updater_->SetActive(false);

    search_controller_.reset();
    app_sync_ui_state_watcher_.reset();
    current_model_updater_ = nullptr;
  }

  template_url_service_observation_.Reset();

  profile_ = new_profile;
  if (!profile_) {
    GetAppListController()->ClearActiveModel();
    return;
  }

  // If we are in guest mode, the new profile should be an OffTheRecord profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use OffTheRecord profile";

  template_url_service_observation_.Observe(
      TemplateURLServiceFactory::GetForProfile(profile_));

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  current_model_updater_ = syncable_service->GetModelUpdater();
  current_model_updater_->SetActive(true);

  // On ChromeOS, there is no way to sign-off just one user. When signing off
  // all users, AppListClientImpl instance is destructed before profiles are
  // unloaded. So we don't need to remove elements from
  // |profile_model_mappings_| explicitly.
  profile_model_mappings_[current_model_updater_->model_id()] =
      current_model_updater_;

  app_sync_ui_state_watcher_ =
      std::make_unique<AppSyncUIStateWatcher>(profile_, current_model_updater_);

  SetUpSearchUI();
  OnTemplateURLServiceChanged();

  // Clear search query.
  current_model_updater_->UpdateSearchBox(std::u16string(),
                                          false /* initiated_by_user */);
}

void AppListClientImpl::SetUpSearchUI() {
  search_controller_ = app_list::CreateSearchController(
      profile_, current_model_updater_, this, GetNotifier());

  // Refresh the results used for the suggestion chips with empty query.
  // This fixes crbug.com/999287.
  StartSearch(std::u16string());
}

app_list::SearchController* AppListClientImpl::search_controller() {
  return search_controller_.get();
}

void AppListClientImpl::SetSearchControllerForTest(
    std::unique_ptr<app_list::SearchController> test_controller) {
  search_controller_ = std::move(test_controller);
}

AppListModelUpdater* AppListClientImpl::GetModelUpdaterForTest() {
  return current_model_updater_;
}

void AppListClientImpl::InitializeAsIfNewUserLoginForTest() {
  new_user_session_activation_time_ = base::Time::Now();
  state_for_new_user_ = StateForNewUser();
}

void AppListClientImpl::OnSessionStateChanged() {
  // Return early if the current user is not new or the session is not active.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew() ||
      !IsSessionActive()) {
    return;
  }

  new_user_session_activation_time_ = base::Time::Now();
}

void AppListClientImpl::OnTemplateURLServiceChanged() {
  DCHECK(current_model_updater_);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool is_google =
      default_provider &&
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  current_model_updater_->SetSearchEngineIsGoogle(is_google);
}

void AppListClientImpl::ShowAppList() {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppList();
}

Profile* AppListClientImpl::GetCurrentAppListProfile() const {
  return ChromeShelfController::instance()->profile();
}

ash::AppListController* AppListClientImpl::GetAppListController() const {
  return app_list_controller_;
}

void AppListClientImpl::DismissView() {
  if (!app_list_controller_)
    return;
  app_list_controller_->DismissAppList();
}

aura::Window* AppListClientImpl::GetAppListWindow() {
  return app_list_controller_->GetWindow();
}

int64_t AppListClientImpl::GetAppListDisplayId() {
  return display_id_;
}

bool AppListClientImpl::IsAppPinned(const std::string& app_id) {
  return ChromeShelfController::instance()->IsAppPinned(app_id);
}

bool AppListClientImpl::IsAppOpen(const std::string& app_id) const {
  return ChromeShelfController::instance()->IsOpen(ash::ShelfID(app_id));
}

void AppListClientImpl::PinApp(const std::string& app_id) {
  PinAppWithIDToShelf(app_id);
}

void AppListClientImpl::UnpinApp(const std::string& app_id) {
  UnpinAppWithIDFromShelf(app_id);
}

AppListControllerDelegate::Pinnable AppListClientImpl::GetPinnable(
    const std::string& app_id) {
  return GetPinnableForAppID(app_id,
                             ChromeShelfController::instance()->profile());
}

void AppListClientImpl::CreateNewWindow(bool incognito,
                                        bool should_trigger_session_restore) {
  ash::NewWindowDelegate::GetInstance()->NewWindow(
      incognito, should_trigger_session_restore);
}

void AppListClientImpl::OpenURL(Profile* profile,
                                const GURL& url,
                                ui::PageTransition transition,
                                WindowOpenDisposition disposition) {
  if (crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    const GURL sanitized_url =
        crosapi::gurl_os_handler_utils::SanitizeAshURL(url);
    if (CanBeHandledAsSystemUrl(sanitized_url, transition)) {
      crosapi::UrlHandlerAsh().OpenUrl(sanitized_url);
    } else {
      // Send the url to the current primary browser.
      ash::NewWindowDelegate::GetPrimary()->OpenUrl(
          url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction);
    }
  } else {
    NavigateParams params(profile, url, transition);
    params.disposition = disposition;
    Navigate(&params);
  }
}

void AppListClientImpl::NotifySearchResultsForLogging(
    const std::u16string& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int position_index) {
  if (search_controller_) {
    search_controller_->OnSearchResultsImpressionMade(trimmed_query, results,
                                                      position_index);
  }
}

ash::AppListNotifier* AppListClientImpl::GetNotifier() {
  return app_list_notifier_.get();
}

void AppListClientImpl::LoadIcon(int profile_id, const std::string& app_id) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }
  requested_model_updater->LoadAppIcon(app_id);
}

ash::AppListSortOrder AppListClientImpl::GetPermanentSortingOrder() const {
  // `profile_` could be set after a user session gets added to the existing
  // session in tests, which does not happen on real devices.
  if (!profile_)
    return ash::AppListSortOrder::kCustom;

  return app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
      ->GetPermanentSortingOrder();
}

void AppListClientImpl::MaybeRecordViewShown() {
  // Record the time duration between session activation and the first launcher
  // showing if the current user is new.

  // We do not need to worry about the scenario below:
  // log in to a new account -> switch to another account -> switch back to the
  // initial account-> show the launcher
  // In this case, when showing the launcher, the current user is not
  // new anymore.
  // TODO(https://crbug.com/1211620): If this bug is fixed, we might need to
  // do some changes here.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew()) {
    DCHECK(!state_for_new_user_);
    return;
  }

  // Record launcher usage only when the session is active.
  // TODO(https://crbug.com/1248250): handle ui events during OOBE in a more
  // elegant way. For example, do not bother showing the app list when handling
  // the app list toggling event because the app list is not visible in OOBE.
  if (!IsSessionActive())
    return;

  // Return early if `state_for_new_user_` is null.
  // TODO(https://crbug.com/1278947): Theoretically, `state_for_new_user_`
  // should be meaningful when the current user is new. However, it is not hold
  // under some edge cases. When the root issue gets fixed, replace it with a
  // check statement.
  if (!state_for_new_user_)
    return;

  if (state_for_new_user_->showing_recorded) {
    // Showing launcher was recorded before so return early.
    return;
  }

  state_for_new_user_->showing_recorded = true;
  state_for_new_user_->shown_in_tablet_mode = IsTabletMode();

  CHECK(new_user_session_activation_time_.has_value());
  const base::TimeDelta opening_duration =
      base::Time::Now() - *new_user_session_activation_time_;
  // `base::Time` may skew. Therefore only record when the time duration is
  // non-negative.
  if (opening_duration >= base::TimeDelta()) {
    if (state_for_new_user_->shown_in_tablet_mode) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps."
          "TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
          "TabletMode",
          /*sample=*/opening_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);

      base::UmaHistogramEnumeration("Apps.AppListUsageByNewUsers.TabletMode",
                                    AppListUsageStateByNewUsers::kUsed);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps."
          "TimeDurationBetweenNewUserSessionActivationAndFirstLauncherOpening."
          "ClamshellMode",
          /*sample=*/opening_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);

      base::UmaHistogramEnumeration("Apps.AppListUsageByNewUsers.ClamshellMode",
                                    AppListUsageStateByNewUsers::kUsed);
    }
  }
}

void AppListClientImpl::RecordOpenedResultFromSearchBox(
    ash::SearchResultType result_type) {
  // Check whether there is any Chrome non-app browser window open and not
  // minimized.
  bool non_app_browser_open_and_not_minimzed = false;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::TYPE_NORMAL ||
        browser->window()->IsMinimized()) {
      // Skip if `browser` is not a normal browser or `browser` is minimized.
      continue;
    }

    non_app_browser_open_and_not_minimzed = true;
    break;
  }

  if (non_app_browser_open_and_not_minimzed) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.OpenedAppListSearchResultFromSearchBoxV2."
        "ExistNonAppBrowserWindowOpenAndNotMinimized",
        result_type, ash::SEARCH_RESULT_TYPE_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.OpenedAppListSearchResultFromSearchBoxV2."
        "NonAppBrowserWindowsEitherClosedOrMinimized",
        result_type, ash::SEARCH_RESULT_TYPE_BOUNDARY);
  }
}

void AppListClientImpl::MaybeRecordLauncherAction(
    ash::AppListLaunchedFrom launched_from) {
  DCHECK(launched_from == ash::AppListLaunchedFrom::kLaunchedFromGrid ||
         launched_from ==
             ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip ||
         launched_from == ash::AppListLaunchedFrom::kLaunchedFromSearchBox ||
         launched_from == ash::AppListLaunchedFrom::kLaunchedFromContinueTask);

  // Return early if the current user is not new.
  if (!user_manager::UserManager::Get()->IsCurrentUserNew()) {
    DCHECK(!state_for_new_user_);
    return;
  }

  // The launcher action has been recorded so return early.
  if (state_for_new_user_->action_recorded)
    return;

  state_for_new_user_->action_recorded = true;
  base::UmaHistogramEnumeration("Apps.FirstLauncherActionByNewUsers",
                                launched_from);
  if (IsTabletMode()) {
    base::UmaHistogramEnumeration(
        "Apps.FirstLauncherActionByNewUsers.TabletMode", launched_from);
  } else {
    base::UmaHistogramEnumeration(
        "Apps.FirstLauncherActionByNewUsers.ClamshellMode", launched_from);
  }

  DCHECK(new_user_session_activation_time_.has_value());
  const base::TimeDelta launcher_action_duration =
      base::Time::Now() - *new_user_session_activation_time_;
  if (launcher_action_duration >= base::TimeDelta()) {
    // `base::Time` may skew. Therefore only record when the time duration is
    // non-negative.
    if (IsTabletMode()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
          "TabletMode",
          /*sample=*/launcher_action_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          /*name=*/
          "Apps.TimeBetweenNewUserSessionActivationAndFirstLauncherAction."
          "ClamshellMode",
          /*sample=*/launcher_action_duration, kTimeMetricsMin, kTimeMetricsMax,
          kTimeMetricsBucketCount);
    }
  }
}
