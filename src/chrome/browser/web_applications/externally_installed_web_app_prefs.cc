// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// The stored preferences look like:
//
// "web_apps": {
//   "extension_ids": {
//     "https://events.google.com/io2016/?utm_source=web_app_manifest": {
//       "extension_id": "mjgafbdfajpigcjmkgmeokfbodbcfijl",
//       "install_source": 1,
//       "is_placeholder": true,
//     },
//     "https://www.chromestatus.com/features": {
//       "extension_id": "fedbieoalmbobgfjapopkghdmhgncnaa",
//       "install_source": 1
//     }
//   }
// }
//
// From the top, prefs::kWebAppsExtensionIDs is "web_apps.extension_ids".
//
// Two levels in is a dictionary (key/value pairs) whose keys are URLs and
// values are leaf dictionaries. Those leaf dictionaries have keys such as
// kExtensionId and kInstallSource.
// The name "extension_id" comes from when PWAs were only backed by the
// Extension system rather than their own. It cannot be changed now that it
// lives persistently in users' profiles.
constexpr char kExtensionId[] = "extension_id";
constexpr char kInstallSource[] = "install_source";
constexpr char kIsPlaceholder[] = "is_placeholder";

// Returns the base::Value in |pref_service| corresponding to our stored dict
// for |app_id|, or nullptr if it doesn't exist.
const base::Value* GetPreferenceValue(const PrefService* pref_service,
                                      const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);
  if (!urls_to_dicts) {
    return nullptr;
  }
  // Do a simple O(N) scan for app_id being a value in each dictionary's
  // key/value pairs. We expect both N and the number of times
  // GetPreferenceValue is called to be relatively small in practice. If they
  // turn out to be large, we can write a more sophisticated implementation.
  for (auto it : urls_to_dicts->DictItems()) {
    const base::Value* root = &it.second;
    const base::Value* v = root;
    if (v->is_dict()) {
      v = v->FindKey(kExtensionId);
      if (v && v->is_string() && (v->GetString() == app_id)) {
        return root;
      }
    }
  }
  return nullptr;
}

}  // namespace

// static
void ExternallyInstalledWebAppPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppsExtensionIDs);
}

// static
bool ExternallyInstalledWebAppPrefs::HasAppId(const PrefService* pref_service,
                                              const AppId& app_id) {
  return GetPreferenceValue(pref_service, app_id) != nullptr;
}

// static
// TODO(crbug.com/1236159): Can be removed after M99.
void ExternallyInstalledWebAppPrefs::RemoveTerminalPWA(
    PrefService* pref_service) {
  DictionaryPrefUpdate update(pref_service, prefs::kWebAppsExtensionIDs);
  update->RemoveKey("chrome-untrusted://terminal/html/pwa.html");
}

// static
bool ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
    const PrefService* pref_service,
    const AppId& app_id,
    ExternalInstallSource install_source) {
  const base::Value* v = GetPreferenceValue(pref_service, app_id);
  if (v == nullptr || !v->is_dict())
    return false;

  v = v->FindKeyOfType(kInstallSource, base::Value::Type::INTEGER);
  return (v && v->GetInt() == static_cast<int>(install_source));
}

// static
std::map<AppId, GURL> ExternallyInstalledWebAppPrefs::BuildAppIdsMap(
    const PrefService* pref_service,
    ExternalInstallSource install_source) {
  const base::Value* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);

  std::map<AppId, GURL> ids_to_urls;

  if (!urls_to_dicts) {
    return ids_to_urls;
  }

  for (auto it : urls_to_dicts->DictItems()) {
    const base::Value* v = &it.second;
    if (!v->is_dict()) {
      continue;
    }

    const base::Value* install_source_value =
        v->FindKeyOfType(kInstallSource, base::Value::Type::INTEGER);
    if (!install_source_value ||
        (install_source_value->GetInt() != static_cast<int>(install_source))) {
      continue;
    }

    v = v->FindKey(kExtensionId);
    if (!v || !v->is_string()) {
      continue;
    }

    GURL url(it.first);
    DCHECK(url.is_valid() && !url.is_empty());
    ids_to_urls[v->GetString()] = url;
  }

  return ids_to_urls;
}

ExternallyInstalledWebAppPrefs::ExternallyInstalledWebAppPrefs(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void ExternallyInstalledWebAppPrefs::Insert(
    const GURL& url,
    const AppId& app_id,
    ExternalInstallSource install_source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kExtensionId, base::Value(app_id));
  dict.SetKey(kInstallSource, base::Value(static_cast<int>(install_source)));

  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  update->SetKey(url.spec(), std::move(dict));
}

bool ExternallyInstalledWebAppPrefs::Remove(const GURL& url) {
  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  return update->RemoveKey(url.spec());
}

absl::optional<AppId> ExternallyInstalledWebAppPrefs::LookupAppId(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* v =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
          ->FindKey(url.spec());
  if (v && v->is_dict()) {
    v = v->FindKey(kExtensionId);
    if (v && v->is_string()) {
      return absl::make_optional(v->GetString());
    }
  }
  return absl::nullopt;
}

bool ExternallyInstalledWebAppPrefs::HasNoApps() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* dict =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs);
  return dict->DictEmpty();
}

absl::optional<AppId> ExternallyInstalledWebAppPrefs::LookupPlaceholderAppId(
    const GURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* entry =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
          ->FindKey(url.spec());
  if (!entry)
    return absl::nullopt;

  absl::optional<bool> is_placeholder = entry->FindBoolKey(kIsPlaceholder);
  if (!is_placeholder.has_value() || !is_placeholder.value())
    return absl::nullopt;

  return *entry->FindStringKey(kExtensionId);
}

void ExternallyInstalledWebAppPrefs::SetIsPlaceholder(const GURL& url,
                                                      bool is_placeholder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
             ->FindKey(url.spec()));
  DictionaryPrefUpdate update(pref_service_, prefs::kWebAppsExtensionIDs);
  base::Value* map = update.Get();

  auto* app_entry = map->FindKey(url.spec());
  DCHECK(app_entry);

  app_entry->SetBoolKey(kIsPlaceholder, is_placeholder);
}

bool ExternallyInstalledWebAppPrefs::IsPlaceholderApp(
    const AppId& app_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value* app_prefs = GetPreferenceValue(pref_service_, app_id);
  if (!app_prefs || !app_prefs->is_dict())
    return false;
  return app_prefs->FindBoolKey(kIsPlaceholder).value_or(false);
}

// static
ExternallyInstalledWebAppPrefs::ParsedPrefs
ExternallyInstalledWebAppPrefs::ParseExternalPrefsToWebAppData(
    PrefService* pref_service) {
  const base::Value* urls_to_dicts =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);
  ParsedPrefs ids_to_parsed_data;
  if (!urls_to_dicts)
    return ids_to_parsed_data;

  for (auto it : urls_to_dicts->DictItems()) {
    const base::Value* v = &it.second;
    if (!v->is_dict()) {
      continue;
    }

    auto* app_id = v->FindKey(kExtensionId);
    if (!app_id || !app_id->is_string()) {
      continue;
    }

    auto* source = v->FindKey(kInstallSource);
    if (!source) {
      continue;
    }

    WebAppManagement::Type source_type = ConvertExternalInstallSourceToSource(
        static_cast<ExternalInstallSource>(source->GetInt()));
    WebApp::ExternalManagementConfig& config =
        ids_to_parsed_data[app_id->GetString()][source_type];
    config.is_placeholder = v->FindBoolKey(kIsPlaceholder).value_or(false);
    config.install_urls.emplace(GURL(it.first));
  }

  return ids_to_parsed_data;
}

// static
void ExternallyInstalledWebAppPrefs::MigrateExternalPrefData(
    PrefService* pref_service,
    WebAppSyncBridge* sync_bridge) {
  ExternallyInstalledWebAppPrefs::ParsedPrefs pref_to_app_data =
      ParseExternalPrefsToWebAppData(pref_service);

  // First migrate data to UserUninstalledPreinstalledWebAppPrefs.
  MigrateExternalPrefDataToPreinstalledPrefs(
      pref_service, &sync_bridge->registrar(), pref_to_app_data);

  ScopedRegistryUpdate update(sync_bridge);
  for (auto it : pref_to_app_data) {
    WebApp* web_app = update->UpdateApp(it.first);
    if (web_app) {
      // Sync data across externally installed prefs and web_app DB.
      for (auto parsed_info : it.second) {
        if (!web_app->GetSources().test(parsed_info.first)) {
          continue;
        }
        web_app->AddPlaceholderInfoToManagementExternalConfigMap(
            parsed_info.first, parsed_info.second.is_placeholder);
        for (auto url : parsed_info.second.install_urls) {
          // Do not migrate invalid URLs.
          DCHECK(url.is_valid());
          web_app->AddInstallURLToManagementExternalConfigMap(parsed_info.first,
                                                              url);
        }
      }
    }
  }
}

// static
void ExternallyInstalledWebAppPrefs::MigrateExternalPrefDataToPreinstalledPrefs(
    PrefService* pref_service,
    const WebAppRegistrar* registrar,
    const ExternallyInstalledWebAppPrefs::ParsedPrefs& parsed_data) {
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(pref_service);
  for (auto pair : parsed_data) {
    const AppId& app_id = pair.first;
    const auto& source_to_config_map = pair.second;
    const auto& it = source_to_config_map.find(WebAppManagement::kDefault);
    // Migration will happen in the following cases:
    // 1. If app_id exists in the external prefs that had source as
    // kDefault but the app is no longer installed in the registry or if it
    // is no longer preinstalled, that means
    // it was preinstalled and then uninstalled by user.
    if (!registrar->IsInstalledByDefaultManagement(app_id) &&
        it != source_to_config_map.end()) {
      preinstalled_prefs.Add(app_id, it->second.install_urls);
    }
    // 2. If the value corresponding to the app_id in
    // web_app::kWasExternalAppUninstalledByUser is true, then it was previously
    // a preinstalled app that was user uninstalled. In this case, we migrate
    // ALL install URLs for that corresponding app_id, because a preinstalled
    // app could have been installed as an app with a different source now.
    if (GetBoolWebAppPref(pref_service, app_id,
                          kWasExternalAppUninstalledByUser)) {
      preinstalled_prefs.Add(app_id, MergeAllUrls(source_to_config_map));
    }
  }
}

// static
base::flat_set<GURL> ExternallyInstalledWebAppPrefs::MergeAllUrls(
    const base::flat_map<WebAppManagement::Type,
                         WebApp::ExternalManagementConfig>& source_config_map) {
  std::vector<GURL> urls;
  for (auto it : source_config_map) {
    for (const GURL& url : it.second.install_urls) {
      DCHECK(url.is_valid());
      urls.push_back(url);
    }
  }

  return urls;
}

}  // namespace web_app
