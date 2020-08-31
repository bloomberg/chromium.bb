// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace crostini {

namespace {

// This prefix is used as a prefix when generating shelf ids for windows we
// couldn't match to an app.
constexpr char kCrostiniShelfIdPrefix[] = "crostini:";
// Prefix of the ApplicationId set on exo windows for X apps.
constexpr char kCrostiniWindowAppIdPrefix[] = "org.chromium.termina.";
// This comes after kCrostiniWindowAppIdPrefix
constexpr char kWMClassPrefix[] = "wmclass.";

const std::string* GetAppNameForWMClass(base::StringPiece wmclass) {
  // A hard-coded mapping from WMClass to app names.
  // This is used to deal with the Linux apps that don't specify the correct
  // WMClass in their desktop files so that their aura windows can be identified
  // with their respective app IDs.
  static const base::NoDestructor<std::map<std::string, std::string>>
      kWMClassToNname({{"Octave-gui", "GNU Octave"},
                       {"MuseScore2", "MuseScore 2"},
                       {"XnViewMP", "XnView Multi Platform"}});
  const auto it = kWMClassToNname->find(wmclass.as_string());
  if (it == kWMClassToNname->end())
    return nullptr;
  return &it->second;
}

bool MatchingString(const std::string& search_string,
                    const std::string& value_string,
                    bool ignore_space) {
  std::string search = search_string;
  std::string value = value_string;
  if (ignore_space) {
    base::RemoveChars(search, " ", &search);
    base::RemoveChars(value, " ", &value);
  }
  return base::EqualsCaseInsensitiveASCII(search, value);
}

enum class FindAppIdResult { NoMatch, UniqueMatch, NonUniqueMatch };
// Looks for an app where prefs_key is set to search_value. Returns the apps id
// if there was only one app matching, otherwise returns an empty string.
FindAppIdResult FindAppId(const base::DictionaryValue* prefs,
                          base::StringPiece prefs_key,
                          base::StringPiece search_value,
                          std::string* result,
                          bool require_startup_notify = false,
                          bool need_display = false,
                          bool ignore_space = false) {
  result->clear();
  for (const auto& item : prefs->DictItems()) {
    if (item.first == GetTerminalId())
      continue;

    if (require_startup_notify &&
        !item.second
             .FindKeyOfType(guest_os::prefs::kAppStartupNotifyKey,
                            base::Value::Type::BOOLEAN)
             ->GetBool())
      continue;

    if (need_display) {
      const base::Value* no_display = item.second.FindKeyOfType(
          guest_os::prefs::kAppNoDisplayKey, base::Value::Type::BOOLEAN);
      if (no_display && no_display->GetBool())
        continue;
    }

    const base::Value* value = item.second.FindKey(prefs_key);
    if (!value)
      continue;
    if (value->type() == base::Value::Type::STRING) {
      if (!MatchingString(search_value.as_string(), value->GetString(),
                          ignore_space)) {
        continue;
      }
    } else if (value->type() == base::Value::Type::DICTIONARY) {
      // Look at the unlocalized name to see if that matches.
      value = value->FindKeyOfType("", base::Value::Type::STRING);
      if (!value || !MatchingString(search_value.as_string(),
                                    value->GetString(), ignore_space)) {
        continue;
      }
    } else {
      continue;
    }

    if (!result->empty())
      return FindAppIdResult::NonUniqueMatch;
    *result = item.first;
  }

  if (!result->empty())
    return FindAppIdResult::UniqueMatch;
  return FindAppIdResult::NoMatch;
}

}  // namespace

// The code follows these steps to identify apps and returns the first match:
// 1) If the Startup Id is set, look for a matching desktop file id.
// 2) Ignore windows if the App Id is not set.
// 3) If the App Id is not prefixed by org.chromium.termina., it's an app with
// native Wayland support. Look for a matching desktop file id.
// 4) If the App Id is prefixed by org.chromium.termina.wmclass.:
// 4.1) Look for an app where StartupWMClass is matches the suffix.
// 4.2) Look for an app where the desktop file id matches the suffix.
// 4.3) Look for an app where the unlocalized name matches the suffix. This
//      handles the xterm & uxterm examples.
// 5) If we couldn't find a match, prefix the app id with 'crostini:' so we can
// easily identify shelf entries as Crostini apps.
std::string GetCrostiniShelfAppId(const Profile* profile,
                                  const std::string* window_app_id,
                                  const std::string* window_startup_id) {
  const base::DictionaryValue* apps =
      profile->GetPrefs()->GetDictionary(guest_os::prefs::kGuestOsRegistry);
  std::string app_id;

  if (window_startup_id) {
    // TODO(timloh): We should use a value that is unique so we can handle
    // an app installed in multiple containers.
    if (FindAppId(apps, guest_os::prefs::kAppDesktopFileIdKey,
                  *window_startup_id, &app_id,
                  true) == FindAppIdResult::UniqueMatch)
      return app_id;
    LOG(ERROR) << "Startup ID was set to '" << *window_startup_id
               << "' but not matched";
    // Try a lookup with the window app id.
  }

  if (!window_app_id)
    return std::string();

  // Wayland apps won't be prefixed with org.chromium.termina.
  if (!base::StartsWith(*window_app_id, kCrostiniWindowAppIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    if (FindAppId(apps, guest_os::prefs::kAppDesktopFileIdKey, *window_app_id,
                  &app_id) == FindAppIdResult::UniqueMatch) {
      return app_id;
    }
    return kCrostiniShelfIdPrefix + *window_app_id;
  }

  base::StringPiece suffix(
      window_app_id->begin() + strlen(kCrostiniWindowAppIdPrefix),
      window_app_id->end());

  // If we don't have an id to match to a desktop file, use the window app id.
  if (!base::StartsWith(suffix, kWMClassPrefix, base::CompareCase::SENSITIVE))
    return kCrostiniShelfIdPrefix + *window_app_id;

  // If an app had StartupWMClass set to the given WM class, use that,
  // otherwise look for a desktop file id matching the WM class.
  base::StringPiece key = suffix.substr(strlen(kWMClassPrefix));
  FindAppIdResult result = FindAppId(
      apps, guest_os::prefs::kAppStartupWMClassKey, key, &app_id,
      false /* require_startup_notification */, true /* need_display */);
  if (result == FindAppIdResult::UniqueMatch)
    return app_id;
  if (result == FindAppIdResult::NonUniqueMatch)
    return kCrostiniShelfIdPrefix + *window_app_id;

  if (FindAppId(apps, guest_os::prefs::kAppDesktopFileIdKey, key, &app_id) ==
      FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  if (FindAppId(apps, guest_os::prefs::kAppNameKey, key, &app_id,
                false /* require_startup_notification */,
                true /* need_display */,
                true /* ignore_space */) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  const std::string* app_name = GetAppNameForWMClass(key);
  if (app_name &&
      FindAppId(apps, guest_os::prefs::kAppNameKey, *app_name, &app_id,
                false /* require_startup_notification */,
                true /* need_display */) == FindAppIdResult::UniqueMatch) {
    return app_id;
  }

  return kCrostiniShelfIdPrefix + *window_app_id;
}

bool IsUnmatchedCrostiniShelfAppId(base::StringPiece shelf_app_id) {
  return base::StartsWith(shelf_app_id, kCrostiniShelfIdPrefix,
                          base::CompareCase::SENSITIVE);
}

bool IsCrostiniShelfAppId(const Profile* profile,
                          base::StringPiece shelf_app_id) {
  if (IsUnmatchedCrostiniShelfAppId(shelf_app_id)) {
    return true;
  }
  if (shelf_app_id == GetTerminalId())
    return true;
  // TODO(timloh): We need to handle desktop files that have been removed.
  // For example, running windows with a no-longer-valid app id will try to
  // use the ExtensionContextMenuModel.
  return profile->GetPrefs()
             ->GetDictionary(guest_os::prefs::kGuestOsRegistry)
             ->FindKey(shelf_app_id) != nullptr;
}

}  // namespace crostini
