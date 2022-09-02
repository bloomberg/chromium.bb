// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/json/json_string_value_serializer.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_api/managed_configuration_store.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace {

const char kManagedConfigurationDirectoryName[] = "Managed Configuration";
// Maximum configuration size is 5MB.
constexpr int kMaxConfigurationFileSize = 5 * 1024 * 1024;
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("managed_configuration_loader",
                                        R"(
        semantics {
          sender: "Web App Managed Configuration Downloader"
          description:
            "Fetches JSON managed configuration from the DM server, for the "
            "origins configured in the ManagedConfigurationPerOrigin policy. "
            "This enables the navigatior.device.getManagedConfiguration(keys) "
            "JavaScript function, to obtain the values of this configuration "
            "that correspond to provided keys. This configuration can only be "
            "set for force-installed web applications, which are defined by "
            "WebAppInstallForceList policy."
          trigger:
            "Changes in the ManagedConfigurationPerOrigin policy."
          data: "No user data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but it will only be "
            "sent if the device administrator sets up a configuration policy "
            "for a particular origin.",
          chrome_policy {
            ManagedConfigurationPerOrigin {
              ManagedConfigurationPerOrigin: "[]"
            }
          }
        })");

// Converts url::Origin into the key that can be used for filenames/dictionary
// keys.
std::string GetOriginEncoded(const url::Origin& origin) {
  std::string serialized = origin.Serialize();
  return base::HexEncode(serialized.data(), serialized.size());
}

}  // namespace

const char ManagedConfigurationAPI::kOriginKey[] = "origin";
const char ManagedConfigurationAPI::kManagedConfigurationUrlKey[] =
    "managed_configuration_url";
const char ManagedConfigurationAPI::kManagedConfigurationHashKey[] =
    "managed_configuration_hash";

class ManagedConfigurationAPI::ManagedConfigurationDownloader {
 public:
  explicit ManagedConfigurationDownloader(const std::string& data_hash)
      : data_hash_(data_hash) {}
  ~ManagedConfigurationDownloader() = default;

  void Fetch(const std::string& data_url,
             base::OnceCallback<void(std::unique_ptr<std::string>)> callback) {
    // URLLoaders should be created at UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL(data_url);

    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);

    simple_loader_->SetRetryOptions(
        /* max_retries=*/3,
        network::SimpleURLLoader::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

    SystemNetworkContextManager* system_network_context_manager =
        g_browser_process->system_network_context_manager();
    network::mojom::URLLoaderFactory* loader_factory =
        system_network_context_manager->GetURLLoaderFactory();

    simple_loader_->DownloadToString(loader_factory, std::move(callback),
                                     kMaxConfigurationFileSize);
  }

  std::string hash() const { return data_hash_; }

 private:
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  const std::string data_hash_;
};

ManagedConfigurationAPI::ManagedConfigurationAPI(Profile* profile)
    : profile_(profile),
      stores_path_(
          profile->GetPath().AppendASCII(kManagedConfigurationDirectoryName)) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kManagedConfigurationPerOrigin,
      base::BindRepeating(
          &ManagedConfigurationAPI::OnConfigurationPolicyChanged,
          weak_ptr_factory_.GetWeakPtr()));
  OnConfigurationPolicyChanged();
}

ManagedConfigurationAPI::~ManagedConfigurationAPI() = default;

// static
void ManagedConfigurationAPI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kManagedConfigurationPerOrigin);
  registry->RegisterDictionaryPref(
      prefs::kLastManagedConfigurationHashForOrigin);
}

void ManagedConfigurationAPI::GetOriginPolicyConfiguration(
    const url::Origin& origin,
    const std::vector<std::string>& keys,
    base::OnceCallback<void(std::unique_ptr<base::DictionaryValue>)> callback) {
  if (!CanHaveManagedStore(origin)) {
    return std::move(callback).Run(nullptr);
  }

  if (!base::Contains(store_map_, origin))
    return std::move(callback).Run(nullptr);

  store_map_[origin]
      .AsyncCall(&ManagedConfigurationStore::Get)
      .WithArgs(std::move(keys))
      .Then(std::move(callback));
}

void ManagedConfigurationAPI::AddObserver(Observer* observer) {
  url::Origin origin = observer->GetOrigin();
  if (CanHaveManagedStore(origin)) {
    MaybeCreateStoreForOrigin(origin);
    observers_[origin].AddObserver(observer);
  } else {
    unmanaged_observers_.insert(observer);
  }
}

void ManagedConfigurationAPI::RemoveObserver(Observer* observer) {
  auto it = unmanaged_observers_.find(observer);
  if (it != unmanaged_observers_.end()) {
    unmanaged_observers_.erase(it);
    return;
  }

  observers_[observer->GetOrigin()].RemoveObserver(observer);
}

bool ManagedConfigurationAPI::CanHaveManagedStore(const url::Origin& origin) {
  return base::Contains(managed_origins_, origin);
}

const std::set<url::Origin>& ManagedConfigurationAPI::GetManagedOrigins()
    const {
  return managed_origins_;
}

void ManagedConfigurationAPI::OnConfigurationPolicyChanged() {
  const base::Value* managed_configurations =
      profile_->GetPrefs()->GetList(prefs::kManagedConfigurationPerOrigin);

  std::set<url::Origin> current_origins;

  for (const auto& entry : managed_configurations->GetListDeprecated()) {
    const std::string* url = entry.FindStringKey(kOriginKey);
    if (!url)
      continue;
    const url::Origin origin = url::Origin::Create(GURL(*url));
    if (origin.opaque())
      continue;

    const std::string* configuration_url =
        entry.FindStringKey(kManagedConfigurationUrlKey);
    const std::string* configuration_hash =
        entry.FindStringKey(kManagedConfigurationHashKey);
    current_origins.insert(origin);

    if (!configuration_url || !configuration_hash)
      continue;
    UpdateStoredDataForOrigin(origin, *configuration_url, *configuration_hash);
  }

  // We need to clean configurations for origins that got their entry removed.
  for (const auto& store_entry : store_map_) {
    if (!base::Contains(current_origins, store_entry.first)) {
      UpdateStoredDataForOrigin(store_entry.first, std::string(),
                                std::string());
    }
  }

  managed_origins_.swap(current_origins);
  PromoteObservers();
}

void ManagedConfigurationAPI::MaybeCreateStoreForOrigin(
    const url::Origin& origin) {
  if (base::Contains(store_map_, origin))
    return;

  // Create the store now, and serve the cached policy until the PolicyService
  // sends updated values.
  store_map_[origin] = base::SequenceBound<ManagedConfigurationStore>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      origin, GetStoreLocation(origin));
}

base::FilePath ManagedConfigurationAPI::GetStoreLocation(
    const url::Origin& origin) {
  return stores_path_.AppendASCII(GetOriginEncoded(origin));
}

void ManagedConfigurationAPI::UpdateStoredDataForOrigin(
    const url::Origin& origin,
    const std::string& configuration_url,
    const std::string& configuration_hash) {
  const std::string* last_hash_value =
      profile_->GetPrefs()
          ->GetDictionary(prefs::kLastManagedConfigurationHashForOrigin)
          ->FindStringKey(GetOriginEncoded(origin));

  // Nothing to be stored here, the hash value is the same.
  if (last_hash_value && *last_hash_value == configuration_hash)
    return;

  if (configuration_url.empty()) {
    PostStoreConfiguration(origin, base::DictionaryValue());
    return;
  }

  // Check whether there is already a downloader.
  if (downloaders_[origin]) {
    // If it downloads the same data already, do nothing.
    if (downloaders_[origin]->hash() == configuration_hash)
      return;
    // Cancel it otherwise.
    downloaders_[origin].reset();
  }

  downloaders_[origin] =
      std::make_unique<ManagedConfigurationDownloader>(configuration_hash);
  downloaders_[origin]->Fetch(
      configuration_url, base::BindOnce(&ManagedConfigurationAPI::DecodeData,
                                        weak_ptr_factory_.GetWeakPtr(), origin,
                                        configuration_hash));
}

void ManagedConfigurationAPI::DecodeData(const url::Origin& origin,
                                         const std::string& url_hash,
                                         std::unique_ptr<std::string> data) {
  downloaders_[origin].reset();
  if (!data)
    return;

  // First, we have to parse JSON file in an isolated sandbox so that we don't
  // have to worry about potentially risky values.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *data,
      base::BindOnce(&ManagedConfigurationAPI::ProcessDecodedConfiguration,
                     weak_ptr_factory_.GetWeakPtr(), origin, url_hash));
}

void ManagedConfigurationAPI::ProcessDecodedConfiguration(
    const url::Origin& origin,
    const std::string& url_hash,
    const data_decoder::DataDecoder::ValueOrError decoding_result) {
  if (!decoding_result.value || !decoding_result.value->is_dict()) {
    VLOG(1) << "Could not fetch managed configuration for app with origin = "
            << origin.Serialize();
    PostStoreConfiguration(origin, base::DictionaryValue());
    return;
  }
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kLastManagedConfigurationHashForOrigin);
  update.Get()->SetStringKey(GetOriginEncoded(origin), url_hash);

  // We need to transform each value into a string.
  base::DictionaryValue result_dict;
  for (auto item : decoding_result.value->DictItems()) {
    std::string result;
    JSONStringValueSerializer serializer(&result);
    serializer.Serialize(item.second);
    result_dict.SetStringPath(item.first, result);
  }

  PostStoreConfiguration(origin, std::move(result_dict));
}

void ManagedConfigurationAPI::PostStoreConfiguration(
    const url::Origin& origin,
    base::DictionaryValue configuration) {
  MaybeCreateStoreForOrigin(origin);
  store_map_[origin]
      .AsyncCall(&ManagedConfigurationStore::SetCurrentPolicy)
      .WithArgs(std::move(configuration))
      .Then(base::BindOnce(
          &ManagedConfigurationAPI::InformObserversIfConfigurationChanged,
          weak_ptr_factory_.GetWeakPtr(), origin));
}

void ManagedConfigurationAPI::InformObserversIfConfigurationChanged(
    const url::Origin& origin,
    bool has_changed) {
  if (!has_changed || !base::Contains(observers_, origin))
    return;

  for (auto& observer : observers_[origin]) {
    observer.OnManagedConfigurationChanged();
  }
}

void ManagedConfigurationAPI::PromoteObservers() {
  for (auto it = unmanaged_observers_.begin();
       it != unmanaged_observers_.end();) {
    if (CanHaveManagedStore((*it)->GetOrigin())) {
      auto* observer = *it;
      it = unmanaged_observers_.erase(it);
      AddObserver(observer);
    } else {
      ++it;
    }
  }
}
