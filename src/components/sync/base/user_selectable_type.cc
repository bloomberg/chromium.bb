// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include <type_traits>

#include "base/notreached.h"
#include "base/optional.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace syncer {

namespace {

struct UserSelectableTypeInfo {
  const char* const type_name;
  const ModelType canonical_model_type;
  const ModelTypeSet model_type_group;
};

constexpr char kBookmarksTypeName[] = "bookmarks";
constexpr char kPreferencesTypeName[] = "preferences";
constexpr char kPasswordsTypeName[] = "passwords";
constexpr char kAutofillTypeName[] = "autofill";
constexpr char kThemesTypeName[] = "themes";
constexpr char kTypedUrlsTypeName[] = "typedUrls";
constexpr char kExtensionsTypeName[] = "extensions";
constexpr char kAppsTypeName[] = "apps";
constexpr char kReadingListTypeName[] = "readingList";
constexpr char kTabsTypeName[] = "tabs";
constexpr char kWifiConfigurationsTypeName[] = "wifiConfigurations";

UserSelectableTypeInfo GetUserSelectableTypeInfo(UserSelectableType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return {kBookmarksTypeName, BOOKMARKS, {BOOKMARKS}};
    case UserSelectableType::kPreferences: {
      ModelTypeSet model_types = {PREFERENCES, DICTIONARY, PRIORITY_PREFERENCES,
                                  SEARCH_ENGINES};
#if defined(OS_CHROMEOS)
      // SplitSettingsSync makes Printers a separate OS setting.
      if (!chromeos::features::IsSplitSettingsSyncEnabled())
        model_types.Put(PRINTERS);
#endif
      return {kPreferencesTypeName, PREFERENCES, model_types};
    }
    case UserSelectableType::kPasswords:
      return {kPasswordsTypeName, PASSWORDS, {PASSWORDS}};
    case UserSelectableType::kAutofill:
      return {kAutofillTypeName,
              AUTOFILL,
              {AUTOFILL, AUTOFILL_PROFILE, AUTOFILL_WALLET_DATA,
               AUTOFILL_WALLET_METADATA}};
    case UserSelectableType::kThemes:
      return {kThemesTypeName, THEMES, {THEMES}};
    case UserSelectableType::kHistory:
      return {kTypedUrlsTypeName,
              TYPED_URLS,
              {TYPED_URLS, HISTORY_DELETE_DIRECTIVES, SESSIONS,
               DEPRECATED_FAVICON_IMAGES, DEPRECATED_FAVICON_TRACKING,
               USER_EVENTS}};
    case UserSelectableType::kExtensions:
      return {
          kExtensionsTypeName, EXTENSIONS, {EXTENSIONS, EXTENSION_SETTINGS}};
    case UserSelectableType::kApps: {
#if defined(OS_CHROMEOS)
      // SplitSettingsSync moves apps to Chrome OS settings.
      if (chromeos::features::IsSplitSettingsSyncEnabled()) {
        return {kAppsTypeName, UNSPECIFIED};
      } else {
        return {kAppsTypeName,
                APPS,
                {APP_LIST, APPS, APP_SETTINGS, ARC_PACKAGE, WEB_APPS}};
      }
#else
      return {kAppsTypeName, APPS, {APPS, APP_SETTINGS, WEB_APPS}};
#endif
    }
    case UserSelectableType::kReadingList:
      return {kReadingListTypeName, READING_LIST, {READING_LIST}};
    case UserSelectableType::kTabs:
      return {kTabsTypeName,
              PROXY_TABS,
              {PROXY_TABS, SESSIONS, DEPRECATED_FAVICON_IMAGES,
               DEPRECATED_FAVICON_TRACKING, SEND_TAB_TO_SELF}};
    case UserSelectableType::kWifiConfigurations: {
#if defined(OS_CHROMEOS)
      // SplitSettingsSync moves Wi-Fi configurations to Chrome OS settings.
      if (chromeos::features::IsSplitSettingsSyncEnabled())
        return {kWifiConfigurationsTypeName, UNSPECIFIED};
#endif
      return {kWifiConfigurationsTypeName,
              WIFI_CONFIGURATIONS,
              {WIFI_CONFIGURATIONS}};
    }
  }
  NOTREACHED();
  return {nullptr, UNSPECIFIED};
}

#if defined(OS_CHROMEOS)
constexpr char kOsAppsTypeName[] = "osApps";
constexpr char kOsPreferencesTypeName[] = "osPreferences";
constexpr char kOsWifiConfigurationsTypeName[] = "osWifiConfigurations";

UserSelectableTypeInfo GetUserSelectableOsTypeInfo(UserSelectableOsType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableOsType::kOsApps:
      return {kOsAppsTypeName,
              APPS,
              {APP_LIST, APPS, APP_SETTINGS, ARC_PACKAGE, WEB_APPS}};
    case UserSelectableOsType::kOsPreferences:
      return {kOsPreferencesTypeName,
              OS_PREFERENCES,
              {OS_PREFERENCES, OS_PRIORITY_PREFERENCES, PRINTERS}};
    case UserSelectableOsType::kOsWifiConfigurations:
      return {kOsWifiConfigurationsTypeName,
              WIFI_CONFIGURATIONS,
              {WIFI_CONFIGURATIONS}};
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

const char* GetUserSelectableTypeName(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).type_name;
}

base::Optional<UserSelectableType> GetUserSelectableTypeFromString(
    const std::string& type) {
  if (type == kBookmarksTypeName) {
    return UserSelectableType::kBookmarks;
  }
  if (type == kPreferencesTypeName) {
    return UserSelectableType::kPreferences;
  }
  if (type == kPasswordsTypeName) {
    return UserSelectableType::kPasswords;
  }
  if (type == kAutofillTypeName) {
    return UserSelectableType::kAutofill;
  }
  if (type == kThemesTypeName) {
    return UserSelectableType::kThemes;
  }
  if (type == kTypedUrlsTypeName) {
    return UserSelectableType::kHistory;
  }
  if (type == kExtensionsTypeName) {
    return UserSelectableType::kExtensions;
  }
  if (type == kAppsTypeName) {
    return UserSelectableType::kApps;
  }
  if (type == kReadingListTypeName) {
    return UserSelectableType::kReadingList;
  }
  if (type == kTabsTypeName) {
    return UserSelectableType::kTabs;
  }
  if (type == kWifiConfigurationsTypeName) {
    return UserSelectableType::kWifiConfigurations;
  }
  return base::nullopt;
}

std::string UserSelectableTypeSetToString(UserSelectableTypeSet types) {
  std::string result;
  for (UserSelectableType type : types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += GetUserSelectableTypeName(type);
  }
  return result;
}

ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).model_type_group;
}

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).canonical_model_type;
}

int UserSelectableTypeToHistogramInt(UserSelectableType type) {
  // TODO(crbug.com/1007293): Use ModelTypeHistogramValue instead of casting to
  // int.
  return static_cast<int>(
      ModelTypeHistogramValue(UserSelectableTypeToCanonicalModelType(type)));
}

#if defined(OS_CHROMEOS)
const char* GetUserSelectableOsTypeName(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).type_name;
}

base::Optional<UserSelectableOsType> GetUserSelectableOsTypeFromString(
    const std::string& type) {
  if (type == kOsAppsTypeName) {
    return UserSelectableOsType::kOsApps;
  }
  if (type == kOsPreferencesTypeName) {
    return UserSelectableOsType::kOsPreferences;
  }
  if (type == kOsWifiConfigurationsTypeName) {
    return UserSelectableOsType::kOsWifiConfigurations;
  }

  // Some pref types migrated from browser prefs to OS prefs. Map the browser
  // type name to the OS type so that enterprise policy SyncTypesListDisabled
  // still applies to the migrated names during SplitSettingsSync roll-out.
  // TODO(https://crbug.com/1059309): Rename "osApps" to "apps" after
  // SplitSettingsSync is the default, and remove the mapping for "preferences".
  if (type == kAppsTypeName) {
    return UserSelectableOsType::kOsApps;
  }
  if (type == kPreferencesTypeName) {
    return UserSelectableOsType::kOsPreferences;
  }
  return base::nullopt;
}

ModelTypeSet UserSelectableOsTypeToAllModelTypes(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).model_type_group;
}

ModelType UserSelectableOsTypeToCanonicalModelType(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).canonical_model_type;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace syncer
