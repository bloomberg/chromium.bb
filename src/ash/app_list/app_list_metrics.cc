// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_metrics.h"

#include <algorithm>
#include <string>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/compositor/compositor.h"

namespace ash {

const char kAppListPeekingToFullscreenHistogram[] =
    "Apps.AppListPeekingToFullscreenSource";

// The UMA histogram that logs smoothness of pagination animation.
constexpr char kPaginationTransitionAnimationSmoothness[] =
    "Apps.PaginationTransition.AnimationSmoothness";
constexpr char kPaginationTransitionAnimationSmoothnessInTablet[] =
    "Apps.PaginationTransition.AnimationSmoothness.TabletMode";
constexpr char kPaginationTransitionAnimationSmoothnessInClamshell[] =
    "Apps.PaginationTransition.AnimationSmoothness.ClamshellMode";

// The UMA histogram that logs which state search results are opened from.
constexpr char kAppListSearchResultOpenSourceHistogram[] =
    "Apps.AppListSearchResultOpenedSource";

// The UMA histogram that logs smoothness of cardified animation.
constexpr char kCardifiedStateAnimationSmoothnessEnter[] =
    "Apps.AppList.CardifiedStateAnimation.AnimationSmoothness."
    "EnterCardifiedState";
constexpr char kCardifiedStateAnimationSmoothnessExit[] =
    "Apps.AppList.CardifiedStateAnimation.AnimationSmoothness."
    "ExitCardifiedState";

// The UMA hisotogram that logs the action user performs on zero state
// search result.
constexpr char kAppListZeroStateSearchResultUserActionHistogram[] =
    "Apps.AppList.ZeroStateSearchResultUserActionType";

// The UMA histogram that logs user's decision(remove or cancel) for zero state
// search result removal confirmation.
constexpr char kAppListZeroStateSearchResultRemovalHistogram[] =
    "Apps.AppList.ZeroStateSearchResultRemovalDecision";

// The base UMA histogram that logs app launches within the HomeLauncher (tablet
// mode AppList), and the fullscreen AppList (when ProductivityLauncher is
// disabled in clamshell mode) and the Shelf.
constexpr char kAppListAppLaunched[] = "Apps.AppListAppLaunchedV2";

// The UMA histograms that log app launches within the AppList, AppListBubble
// and Shelf. The app launches are divided by histogram for each of the the
// different AppList states.
constexpr char kAppListAppLaunchedBubbleAllApps[] =
    "Apps.AppListAppLaunchedV2.BubbleAllApps";
constexpr char kAppListAppLaunchedClosed[] = "Apps.AppListAppLaunchedV2.Closed";
constexpr char kAppListAppLaunchedPeeking[] =
    "Apps.AppListAppLaunchedV2.Peeking";
constexpr char kAppListAppLaunchedHalf[] = "Apps.AppListAppLaunchedV2.Half";
constexpr char kAppListAppLaunchedFullscreenAllApps[] =
    "Apps.AppListAppLaunchedV2.FullscreenAllApps";
constexpr char kAppListAppLaunchedFullscreenSearch[] =
    "Apps.AppListAppLaunchedV2.FullscreenSearch";
constexpr char kAppListAppLaunchedHomecherClosed[] =
    "Apps.AppListAppLaunchedV2.HomecherClosed";
constexpr char kAppListAppLaunchedHomecherAllApps[] =
    "Apps.AppListAppLaunchedV2.HomecherAllApps";
constexpr char kAppListAppLaunchedHomecherSearch[] =
    "Apps.AppListAppLaunchedV2.HomecherSearch";

// The prefix for all the variants that track how long the app list is kept
// open by open method. Suffix is decided in `GetAppListOpenMethod`
constexpr char kAppListOpenTimePrefix[] = "Apps.AppListOpenTime.";

// The different sources from which a search result is displayed. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum class ApplistSearchResultOpenedSource {
  kHalfClamshell = 0,
  kFullscreenClamshell = 1,
  kFullscreenTablet = 2,
  kMaxApplistSearchResultOpenedSource = 3,
};

void AppListRecordPageSwitcherSourceByEventType(ui::EventType type,
                                                bool is_tablet_mode) {
  AppListPageSwitcherSource source;

  switch (type) {
    case ui::ET_MOUSEWHEEL:
      source = kMouseWheelScroll;
      break;
    case ui::ET_SCROLL:
      source = kMousePadScroll;
      break;
    case ui::ET_GESTURE_SCROLL_END:
      source = kSwipeAppGrid;
      break;
    case ui::ET_SCROLL_FLING_START:
      source = kFlingAppGrid;
      break;
    case ui::ET_MOUSE_RELEASED:
      source = kMouseDrag;
      break;
    default:
      NOTREACHED();
      return;
  }
  RecordPageSwitcherSource(source, is_tablet_mode);
}

void RecordPageSwitcherSource(AppListPageSwitcherSource source,
                              bool is_tablet_mode) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListPageSwitcherSource", source,
                            kMaxAppListPageSwitcherSource);
  if (is_tablet_mode) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListPageSwitcherSource.TabletMode",
                              source, kMaxAppListPageSwitcherSource);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListPageSwitcherSource.ClamshellMode",
                              source, kMaxAppListPageSwitcherSource);
  }
}

void RecordSearchResultOpenSource(const SearchResult* result,
                                  const AppListModel* model,
                                  const SearchModel* search_model) {
  // Record the search metric if the SearchResult is not a suggested app.
  if (result->is_recommendation())
    return;

  ApplistSearchResultOpenedSource source;
  AppListViewState state = model->state_fullscreen();
  if (search_model->tablet_mode()) {
    source = ApplistSearchResultOpenedSource::kFullscreenTablet;
  } else {
    source = state == AppListViewState::kHalf
                 ? ApplistSearchResultOpenedSource::kHalfClamshell
                 : ApplistSearchResultOpenedSource::kFullscreenClamshell;
  }
  UMA_HISTOGRAM_ENUMERATION(
      kAppListSearchResultOpenSourceHistogram, source,
      ApplistSearchResultOpenedSource::kMaxApplistSearchResultOpenedSource);
}

void RecordZeroStateSearchResultUserActionHistogram(
    ZeroStateSearchResultUserActionType action) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSearchResultUserActionHistogram,
                            action);
}

void RecordZeroStateSearchResultRemovalHistogram(
    ZeroStateSearchResutRemovalConfirmation removal_decision) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSearchResultRemovalHistogram,
                            removal_decision);
}

std::string GetAppListOpenMethod(AppListShowSource source) {
  // This switch determines which metric we submit for the Apps.AppListOpenTime
  // metric. Adding a string requires you update the apps histogram.xml as well.
  switch (source) {
    case kSearchKey:
    case kSearchKeyFullscreen:
      return "SearchKey";
    case kShelfButton:
    case kShelfButtonFullscreen:
      return "HomeButton";
    case kSwipeFromShelf:
      return "Swipe";
    case kScrollFromShelf:
      return "Scroll";
    case kTabletMode:
    case kAssistantEntryPoint:
      return "Others";
  }
  NOTREACHED();
}

void RecordAppListUserJourneyTime(AppListShowSource source,
                                  base::TimeDelta time) {
  base::UmaHistogramMediumTimes(
      kAppListOpenTimePrefix + GetAppListOpenMethod(source), time);
}

void RecordPeriodicAppListMetrics() {
  int number_of_apps_in_launcher = 0;
  int number_of_root_level_items = 0;

  AppListItemList* item_list =
      Shell::Get()->app_list_controller()->GetModel()->top_level_item_list();
  for (size_t i = 0; i < item_list->item_count(); ++i) {
    AppListItem* item = item_list->item_at(i);
    if (item->GetItemType() == AppListFolderItem::kItemType) {
      AppListFolderItem* folder = static_cast<AppListFolderItem*>(item);
      number_of_apps_in_launcher += folder->item_list()->item_count();
      number_of_root_level_items++;
    } else if (!item->is_page_break()) {
      number_of_apps_in_launcher++;
      number_of_root_level_items++;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Apps.AppList.NumberOfApps",
                           number_of_apps_in_launcher);
  UMA_HISTOGRAM_COUNTS_100("Apps.AppList.NumberOfRootLevelItems",
                           number_of_root_level_items);
}

void RecordAppListAppLaunched(AppListLaunchedFrom launched_from,
                              AppListViewState app_list_state,
                              bool is_tablet_mode,
                              bool app_list_shown) {
  UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunched, launched_from);

  if (features::IsProductivityLauncherEnabled() && !is_tablet_mode) {
    if (!app_list_shown) {
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedClosed, launched_from);
    } else {
      // TODO(newcomer): Handle the case where search is open.
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedBubbleAllApps,
                                launched_from);
    }
    return;
  }

  switch (app_list_state) {
    case AppListViewState::kClosed:
      DCHECK(!features::IsProductivityLauncherEnabled());
      // Only exists in clamshell mode with ProductivityLauncher disabled.
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedClosed, launched_from);
      break;
    case AppListViewState::kPeeking:
      DCHECK(!features::IsProductivityLauncherEnabled());
      // Only exists in clamshell mode with ProductivityLauncher disabled.
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedPeeking, launched_from);
      break;
    case AppListViewState::kHalf:
      DCHECK(!features::IsProductivityLauncherEnabled());
      // Only exists in clamshell mode with ProductivityLauncher disabled.
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHalf, launched_from);
      break;
    case AppListViewState::kFullscreenAllApps:
      if (is_tablet_mode) {
        if (app_list_shown) {
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherAllApps,
                                    launched_from);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherClosed,
                                    launched_from);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedFullscreenAllApps,
                                  launched_from);
      }
      break;
    case AppListViewState::kFullscreenSearch:
      if (is_tablet_mode) {
        if (app_list_shown) {
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherSearch,
                                    launched_from);
        } else {
          // (http://crbug.com/947729) Search box still expanded when opening
          // launcher in tablet mode
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherClosed,
                                    launched_from);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedFullscreenSearch,
                                  launched_from);
      }
      break;
  }
}

bool IsCommandIdAnAppLaunch(int command_id_number) {
  CommandId command_id = static_cast<CommandId>(command_id_number);

  // Consider all platform app menu options as launches.
  if (command_id >= CommandId::EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id < CommandId::EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return true;
  }

  // Consider all arc app shortcut options as launches.
  if (command_id >= CommandId::LAUNCH_APP_SHORTCUT_FIRST &&
      command_id < CommandId::LAUNCH_APP_SHORTCUT_LAST) {
    return true;
  }

  // All app menu items in a ShelfApplicationMenuModel are not launches.
  if (command_id >= CommandId::APP_MENU_ITEM_ID_FIRST &&
      command_id < CommandId::APP_MENU_ITEM_ID_LAST) {
    return false;
  }

  switch (command_id) {
    // Used by ShelfContextMenu (shelf).
    case CommandId::MENU_OPEN_NEW:
    case CommandId::MENU_NEW_WINDOW:
    case CommandId::MENU_NEW_INCOGNITO_WINDOW:
    // Used by AppContextMenu and/or ShelfContextMenu.
    case CommandId::LAUNCH_NEW:
    case CommandId::SHOW_APP_INFO:
    case CommandId::OPTIONS:
    case CommandId::APP_CONTEXT_MENU_NEW_WINDOW:
    case CommandId::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
    case CommandId::SETTINGS:
    // Used by both AppContextMenu and ShelfContextMenu for app shortcuts.
    case CommandId::LAUNCH_APP_SHORTCUT_FIRST:
    case CommandId::LAUNCH_APP_SHORTCUT_LAST:
      return true;

    // Used by ShelfContextMenu (shelf).
    case CommandId::MENU_CLOSE:
    case CommandId::MENU_PIN:
    case CommandId::LAUNCH_TYPE_PINNED_TAB:
    case CommandId::LAUNCH_TYPE_REGULAR_TAB:
    case CommandId::LAUNCH_TYPE_FULLSCREEN:
    case CommandId::LAUNCH_TYPE_WINDOW:
    case CommandId::LAUNCH_TYPE_TABBED_WINDOW:
    case CommandId::SWAP_WITH_NEXT:
    case CommandId::SWAP_WITH_PREVIOUS:
    // Used by AppMenuModelAdapter
    case CommandId::NOTIFICATION_CONTAINER:
    // Used by CrostiniShelfContextMenu.
    case CommandId::CROSTINI_USE_LOW_DENSITY:
    case CommandId::CROSTINI_USE_HIGH_DENSITY:
    // Used by AppContextMenu.
    case CommandId::TOGGLE_PIN:
    case CommandId::UNINSTALL:
    case CommandId::REMOVE_FROM_FOLDER:
    case CommandId::INSTALL:
    case CommandId::USE_LAUNCH_TYPE_PINNED:
    case CommandId::USE_LAUNCH_TYPE_REGULAR:
    case CommandId::USE_LAUNCH_TYPE_FULLSCREEN:
    case CommandId::USE_LAUNCH_TYPE_WINDOW:
    case CommandId::USE_LAUNCH_TYPE_TABBED_WINDOW:
    case CommandId::USE_LAUNCH_TYPE_COMMAND_END:
    case CommandId::SHUTDOWN_GUEST_OS:
    case CommandId::EXTENSIONS_CONTEXT_CUSTOM_FIRST:
    case CommandId::EXTENSIONS_CONTEXT_CUSTOM_LAST:
    case CommandId::COMMAND_ID_COUNT:
    // Used by ShelfApplicationMenuModel.
    case CommandId::APP_MENU_ITEM_ID_FIRST:
    case CommandId::APP_MENU_ITEM_ID_LAST:
      return false;
  }
  NOTREACHED();
  return false;
}

void ReportPaginationSmoothness(bool is_tablet_mode, int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE(kPaginationTransitionAnimationSmoothness,
                           smoothness);

  if (is_tablet_mode) {
    UMA_HISTOGRAM_PERCENTAGE(kPaginationTransitionAnimationSmoothnessInTablet,
                             smoothness);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        kPaginationTransitionAnimationSmoothnessInClamshell, smoothness);
  }
}

void ReportCardifiedSmoothness(bool is_entering_cardified, int smoothness) {
  if (is_entering_cardified) {
    UMA_HISTOGRAM_PERCENTAGE(kCardifiedStateAnimationSmoothnessEnter,
                             smoothness);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(kCardifiedStateAnimationSmoothnessExit,
                             smoothness);
  }
}

}  // namespace ash
