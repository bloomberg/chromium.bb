// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "extensions/common/constants.h"

namespace {

// Chrome is pinned explicitly.
const char* kDefaultPinnedApps[] = {
    extension_misc::kGmailAppId, extension_misc::kGoogleDocAppId,
    extension_misc::kYoutubeAppId, arc::kPlayStoreAppId};

const char* kDefaultPinnedApps7Apps[] = {
    extension_misc::kGmailAppId,        extension_misc::kGoogleDocAppId,
    extension_misc::kGooglePhotosAppId, extension_misc::kFilesManagerAppId,
    extension_misc::kYoutubeAppId,      arc::kPlayStoreAppId};

const char* kDefaultPinnedApps10Apps[] = {extension_misc::kGmailAppId,
                                          extension_misc::kCalendarAppId,
                                          extension_misc::kGoogleDocAppId,
                                          extension_misc::kGoogleSheetsAppId,
                                          extension_misc::kGoogleSlidesAppId,
                                          extension_misc::kFilesManagerAppId,
                                          app_list::kInternalAppIdCamera,
                                          extension_misc::kGooglePhotosAppId,
                                          arc::kPlayStoreAppId};

const char kDefaultPinnedAppsKey[] = "default";
const char kDefaultPinnedApps7AppsKey[] = "7apps";
const char kDefaultPinnedApps10AppsKey[] = "10apps";

bool IsAppIdArcPackage(const std::string& app_id) {
  return app_id.find('.') != app_id.npos;
}

std::vector<std::string> GetActivitiesForPackage(
    const std::string& package,
    const std::vector<std::string>& all_arc_app_ids,
    const ArcAppListPrefs& app_list_pref) {
  std::vector<std::string> activities;
  for (const std::string& app_id : all_arc_app_ids) {
    const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_list_pref.GetApp(app_id);
    if (app_info->package_name == package) {
      activities.push_back(app_info->activity);
    }
  }
  return activities;
}

std::vector<ash::ShelfID> AppIdsToShelfIDs(
    const std::vector<std::string> app_ids) {
  std::vector<ash::ShelfID> shelf_ids(app_ids.size());
  for (size_t i = 0; i < app_ids.size(); ++i)
    shelf_ids[i] = ash::ShelfID(app_ids[i]);
  return shelf_ids;
}

struct PinInfo {
  PinInfo(const std::string& app_id, const syncer::StringOrdinal& item_ordinal)
      : app_id(app_id), item_ordinal(item_ordinal) {}

  std::string app_id;
  syncer::StringOrdinal item_ordinal;
};

struct ComparePinInfo {
  bool operator()(const PinInfo& pin1, const PinInfo& pin2) {
    return pin1.item_ordinal.LessThan(pin2.item_ordinal);
  }
};

// Returns true in case default pin layout |default_pin_layout| was already
// rolled.
bool IsDefaultPinLayoutRolled(Profile* profile,
                              const std::string& default_pin_layout) {
  const auto* layouts_rolled =
      profile->GetPrefs()->GetList(prefs::kShelfDefaultPinLayoutRolls);
  if (!layouts_rolled)
    return false;

  for (const auto& layout_rolled : layouts_rolled->GetList()) {
    if (layout_rolled.GetString() == default_pin_layout)
      return true;
  }

  return false;
}

// Marks that default pin layout |default_pin_layout| is rolled.
void MarkDefaultPinLayoutRolled(Profile* profile,
                                const std::string& default_pin_layout) {
  DCHECK(!IsDefaultPinLayoutRolled(profile, default_pin_layout));

  ListPrefUpdate update(profile->GetPrefs(),
                        prefs::kShelfDefaultPinLayoutRolls);
  update->AppendString(default_pin_layout);
}

// Returns true in case default pin layout configuration could be applied
// safely. That means all required components are synced or worked in local
// mode.
bool IsSafeToApplyDefaultPinLayout(Profile* profile) {
  const syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  // No |sync_service| in incognito mode.
  if (!sync_service)
    return true;

  const syncer::UserSelectableTypeSet selected_sync =
      sync_service->GetUserSettings()->GetSelectedTypes();

  // If App sync is not yet started, don't apply default pin apps once synced
  // apps is likely override it. There is a case when App sync is disabled and
  // in last case local cache is available immediately.
  if (selected_sync.Has(syncer::UserSelectableType::kApps) &&
      !app_list::AppListSyncableServiceFactory::GetForProfile(profile)
           ->IsSyncing()) {
    return false;
  }

  // If shelf pin layout rolls preference is not started yet then we cannot say
  // if we rolled layout or not.
  if (selected_sync.Has(syncer::UserSelectableType::kPreferences) &&
      !PrefServiceSyncableFromProfile(profile)->IsSyncing()) {
    return false;
  }

  return true;
}

}  // namespace

const char kPinnedAppsPrefAppIDKey[] = "id";

void RegisterChromeLauncherUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPolicyPinnedLauncherApps);
  registry->RegisterListPref(
      prefs::kShelfDefaultPinLayoutRolls,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
}

void InitLocalPref(PrefService* prefs, const char* local, const char* synced) {
  // Ash's prefs *should* have been propagated to Chrome by now, but maybe not.
  // This belongs in Ash, but it can't observe syncing changes: crbug.com/774657
  if (prefs->FindPreference(local) && prefs->FindPreference(synced) &&
      !prefs->FindPreference(local)->HasUserSetting()) {
    prefs->SetString(local, prefs->GetString(synced));
  }
}

// Helper that extracts app list from policy preferences.
std::vector<std::string> GetAppsPinnedByPolicy(
    const LauncherControllerHelper* helper) {
  const PrefService* prefs = helper->profile()->GetPrefs();
  std::vector<std::string> result;
  const auto* policy_apps = prefs->GetList(prefs::kPolicyPinnedLauncherApps);
  if (!policy_apps)
    return result;

  // Obtain here all ids of ARC apps because it takes linear time, and getting
  // them in the loop bellow would lead to quadratic complexity.
  const ArcAppListPrefs* const arc_app_list_pref = helper->GetArcAppListPrefs();
  const std::vector<std::string> all_arc_app_ids(
      arc_app_list_pref ? arc_app_list_pref->GetAppIds()
                        : std::vector<std::string>());

  for (const auto& policy_apps_entry : policy_apps->GetList()) {
    const base::Value* app_id_value =
        policy_apps_entry.is_dict()
            ? policy_apps_entry.FindKeyOfType(kPinnedAppsPrefAppIDKey,
                                              base::Value::Type::STRING)
            : nullptr;
    if (!app_id_value) {
      LOG(ERROR) << "Cannot extract policy app info from prefs.";
      continue;
    }
    const std::string app_id = app_id_value->GetString();

    if (chromeos::DemoSession::Get() &&
        chromeos::DemoSession::Get()->ShouldIgnorePinPolicy(app_id)) {
      continue;
    }

    if (IsAppIdArcPackage(app_id)) {
      if (!arc_app_list_pref)
        continue;

      // We are dealing with package name, not with 32 characters ID.
      const std::string& arc_package = app_id;
      const std::vector<std::string> activities = GetActivitiesForPackage(
          arc_package, all_arc_app_ids, *arc_app_list_pref);
      for (const auto& activity : activities) {
        const std::string arc_app_id =
            ArcAppListPrefs::GetAppId(arc_package, activity);
        result.emplace_back(arc_app_id);
      }
    } else {
      result.emplace_back(app_id);
    }
  }
  return result;
}

// Returns pinned app position even if app is not currently visible on device
// that is leftmost item on the shelf. If |exclude_chrome| is true then Chrome
// app is not processed. if nothing pinned found, returns an invalid ordinal.
syncer::StringOrdinal GetFirstPinnedAppPosition(Profile* profile,
                                                bool exclude_chrome) {
  syncer::StringOrdinal position;
  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  for (const auto& sync_peer : app_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;
    if (exclude_chrome && sync_peer.first == extension_misc::kChromeAppId)
      continue;
    if (!position.IsValid() ||
        sync_peer.second->item_pin_ordinal.LessThan(position)) {
      position = sync_peer.second->item_pin_ordinal;
    }
  }
  return position;
}

// Helper to create pin position that stays before any synced app, even if
// app is not currently visible on a device.
syncer::StringOrdinal CreateFirstPinPosition(Profile* profile) {
  const syncer::StringOrdinal position =
      GetFirstPinnedAppPosition(profile, false /* exclude_chrome */);
  return position.IsValid() ? position.CreateBefore()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
}

// Helper to creates pin position that stays before any synced app, even if
// app is not currently visible on a device.
syncer::StringOrdinal CreateLastPinPosition(Profile* profile) {
  syncer::StringOrdinal position;
  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  for (const auto& sync_peer : app_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;
    if (!position.IsValid() ||
        sync_peer.second->item_pin_ordinal.GreaterThan(position)) {
      position = sync_peer.second->item_pin_ordinal;
    }
  }

  return position.IsValid() ? position.CreateAfter()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
}

// Helper to create and insert pins on the shelf for the set of apps defined in
// |app_ids| after Chrome in the first position and before any other pinned app.
// If Chrome is not the first pinned app then apps are pinned before any other
// app.
void InsertPinsAfterChromeAndBeforeFirstPinnedApp(
    LauncherControllerHelper* helper,
    app_list::AppListSyncableService* app_service,
    const std::vector<std::string>& app_ids,
    std::vector<PinInfo>* pin_infos) {
  if (app_ids.empty())
    return;

  // Chrome must be pinned at this point.
  const syncer::StringOrdinal chrome_position =
      app_service->GetPinPosition(extension_misc::kChromeAppId);
  DCHECK(chrome_position.IsValid());

  // New pins are inserted after this position.
  syncer::StringOrdinal after;
  // New pins are inserted before this position.
  syncer::StringOrdinal before =
      GetFirstPinnedAppPosition(helper->profile(), true /* exclude_chrome */);

  if (before.IsValid() && before.GreaterThan(chrome_position)) {
    // Perfect case, Chrome is the first pinned app and we have next pinned app.
    after = chrome_position;
  } else {
    if (before.IsValid()) {
      // Chrome is not first. Generate after as position before |before|.
      after = before.CreateBefore();
    } else {
      // Chrome is first and no more other pinned apps or apps are
      // conflicting with Chrome pin position.
      after = chrome_position;
      before = chrome_position.CreateAfter();
    }
  }

  for (const auto& app_id : app_ids) {
    // Check if we already processed the current app.
    if (app_service->GetPinPosition(app_id).IsValid())
      continue;

    const syncer::StringOrdinal position = after.CreateBetween(before);
    app_service->SetPinPosition(app_id, position);

    // Even if app is not currently visible, its pin is pre-created.
    if (helper->IsValidIDForCurrentUser(app_id))
      pin_infos->emplace_back(PinInfo(app_id, position));

    // Shift after position, next policy pin position will be created after
    // current item.
    after = position;
  }
}

std::vector<ash::ShelfID> GetPinnedAppsFromSync(
    LauncherControllerHelper* helper) {
  const PrefService* prefs = helper->profile()->GetPrefs();
  app_list::AppListSyncableService* const app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(helper->profile());
  // Some unit tests may not have it or service may not be initialized.
  if (!app_service || !app_service->IsInitialized())
    return std::vector<ash::ShelfID>();

  std::vector<PinInfo> pin_infos;

  // Empty pins indicates that sync based pin model is used for the first
  // time. In the normal workflow we have at least Chrome browser pin info.

  for (const auto& sync_peer : app_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;

    // Don't include apps that currently do not exist on device.
    if (sync_peer.first != extension_misc::kChromeAppId &&
        !helper->IsValidIDForCurrentUser(sync_peer.first)) {
      continue;
    }

    // Prevent old app camera pinning.
    if (IsCameraApp(sync_peer.first))
      continue;

    pin_infos.emplace_back(
        PinInfo(sync_peer.first, sync_peer.second->item_pin_ordinal));
  }

  // Make sure Chrome is always pinned.
  syncer::StringOrdinal chrome_position =
      app_service->GetPinPosition(extension_misc::kChromeAppId);
  if (!chrome_position.IsValid()) {
    chrome_position = CreateFirstPinPosition(helper->profile());
    app_service->SetPinPosition(extension_misc::kChromeAppId, chrome_position);
    pin_infos.emplace_back(
        PinInfo(extension_misc::kChromeAppId, chrome_position));
  }

  // Apply default apps in case profile syncing is done. Otherwise there is a
  // risk that applied default apps would be overwritten by sync once it is
  // completed. prefs::kPolicyPinnedLauncherApps overrides any default layout.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string shelf_layout = kDefaultPinnedAppsKey;
  if (command_line->HasSwitch(switches::kPinShelfLayout)) {
    const std::string forced_shelf_layout =
        command_line->GetSwitchValueASCII(switches::kPinShelfLayout);
    if (forced_shelf_layout == kDefaultPinnedAppsKey ||
        forced_shelf_layout == kDefaultPinnedApps7AppsKey ||
        forced_shelf_layout == kDefaultPinnedApps10AppsKey) {
      shelf_layout = forced_shelf_layout;
    } else {
      LOG(ERROR) << "Wrong default shelf pin layout " << forced_shelf_layout;
    }
  }

  if (!prefs->HasPrefPath(prefs::kPolicyPinnedLauncherApps) &&
      IsSafeToApplyDefaultPinLayout(helper->profile()) &&
      !IsDefaultPinLayoutRolled(helper->profile(), shelf_layout)) {
    VLOG(1) << "Roll default shelf pin layout " << shelf_layout;
    std::vector<std::string> default_app_ids;
    if (shelf_layout == kDefaultPinnedApps7AppsKey) {
      for (const char* default_app_id : kDefaultPinnedApps7Apps)
        default_app_ids.push_back(default_app_id);
    } else if (shelf_layout == kDefaultPinnedApps10AppsKey) {
      for (const char* default_app_id : kDefaultPinnedApps10Apps)
        default_app_ids.push_back(default_app_id);
    } else {
      for (const char* default_app_id : kDefaultPinnedApps)
        default_app_ids.push_back(default_app_id);
    }
    InsertPinsAfterChromeAndBeforeFirstPinnedApp(helper, app_service,
                                                 default_app_ids, &pin_infos);
    MarkDefaultPinLayoutRolled(helper->profile(), shelf_layout);
  }

  // Handle pins, forced by policy. In case Chrome is first app they are added
  // after Chrome, otherwise they are added to the front. Note, we handle apps
  // that may not be currently on device. At this case pin position would be
  // preallocated and apps will appear on shelf in deterministic order, even if
  // their install order differ.
  InsertPinsAfterChromeAndBeforeFirstPinnedApp(
      helper, app_service, GetAppsPinnedByPolicy(helper), &pin_infos);

  // Sort pins according their ordinals.
  std::sort(pin_infos.begin(), pin_infos.end(), ComparePinInfo());

  // Convert to ShelfID array.
  std::vector<std::string> pins(pin_infos.size());
  for (size_t i = 0; i < pin_infos.size(); ++i)
    pins[i] = pin_infos[i].app_id;

  return AppIdsToShelfIDs(pins);
}

void RemovePinPosition(Profile* profile, const ash::ShelfID& shelf_id) {
  DCHECK(profile);

  const std::string& app_id = shelf_id.app_id;
  if (!shelf_id.launch_id.empty()) {
    VLOG(2) << "Syncing remove pin for '" << app_id
            << "' with non-empty launch id '" << shelf_id.launch_id
            << "' is not supported.";
    return;
  }
  DCHECK(!app_id.empty());

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  app_service->SetPinPosition(app_id, syncer::StringOrdinal());
}

void SetPinPosition(Profile* profile,
                    const ash::ShelfID& shelf_id,
                    const ash::ShelfID& shelf_id_before,
                    const std::vector<ash::ShelfID>& shelf_ids_after) {
  DCHECK(profile);
  // Camera apps are mapped to the internal app.
  DCHECK(!IsCameraApp(shelf_id.app_id));

  const std::string& app_id = shelf_id.app_id;
  if (!shelf_id.launch_id.empty()) {
    VLOG(2) << "Syncing set pin for '" << app_id
            << "' with non-empty launch id '" << shelf_id.launch_id
            << "' is not supported.";
    return;
  }

  const std::string& app_id_before = shelf_id_before.app_id;

  DCHECK(!app_id.empty());
  DCHECK_NE(app_id, app_id_before);

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  // Some unit tests may not have this service.
  if (!app_service)
    return;

  syncer::StringOrdinal position_before =
      app_id_before.empty() ? syncer::StringOrdinal()
                            : app_service->GetPinPosition(app_id_before);
  syncer::StringOrdinal position_after;
  for (const auto& shelf_id_after : shelf_ids_after) {
    const std::string& app_id_after = shelf_id_after.app_id;
    DCHECK_NE(app_id_after, app_id);
    DCHECK_NE(app_id_after, app_id_before);
    syncer::StringOrdinal position = app_service->GetPinPosition(app_id_after);
    DCHECK(position.IsValid());
    if (!position.IsValid()) {
      LOG(ERROR) << "Sync pin position was not found for " << app_id_after;
      continue;
    }
    if (!position_before.IsValid() || !position.Equals(position_before)) {
      position_after = position;
      break;
    }
  }

  syncer::StringOrdinal pin_position;
  if (position_before.IsValid() && position_after.IsValid())
    pin_position = position_before.CreateBetween(position_after);
  else if (position_before.IsValid())
    pin_position = position_before.CreateAfter();
  else if (position_after.IsValid())
    pin_position = position_after.CreateBefore();
  else
    pin_position = syncer::StringOrdinal::CreateInitialOrdinal();
  app_service->SetPinPosition(app_id, pin_position);
}
