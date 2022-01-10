// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache_impl.h"

#include <stddef.h>
#include <utility>

#include "ash/components/settings/cros_settings_names.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

void FlushFile(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  file.Flush();
  file.Close();
}

}  // namespace

ExternalCacheImpl::ExternalCacheImpl(
    const base::FilePath& cache_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    ExternalCacheDelegate* delegate,
    bool always_check_updates,
    bool wait_for_cache_initialization,
    bool allow_scheduled_updates)
    : local_cache_(cache_dir, 0, base::TimeDelta(), backend_task_runner),
      url_loader_factory_(std::move(url_loader_factory)),
      backend_task_runner_(backend_task_runner),
      delegate_(delegate),
      always_check_updates_(always_check_updates),
      wait_for_cache_initialization_(wait_for_cache_initialization),
      allow_scheduled_updates_(allow_scheduled_updates),
      cached_extensions_(new base::DictionaryValue()) {
  notification_registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
  kiosk_crx_updates_from_policy_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          ash::kKioskCRXManifestUpdateURLIgnored,
          base::BindRepeating(&ExternalCacheImpl::MaybeScheduleNextCacheCheck,
                              weak_ptr_factory_.GetWeakPtr()));
}

ExternalCacheImpl::~ExternalCacheImpl() = default;

const base::DictionaryValue* ExternalCacheImpl::GetCachedExtensions() {
  return cached_extensions_.get();
}

void ExternalCacheImpl::Shutdown(base::OnceClosure callback) {
  local_cache_.Shutdown(std::move(callback));
}

void ExternalCacheImpl::UpdateExtensionsList(
    std::unique_ptr<base::DictionaryValue> prefs) {
  extensions_ = std::move(prefs);

  if (extensions_->DictEmpty()) {
    // If list of know extensions is empty, don't init cache on disk. It is
    // important shortcut for test to don't wait forever for cache dir
    // initialization that should happen outside of Chrome on real device.
    cached_extensions_->DictClear();
    UpdateExtensionLoader();
    return;
  }

  CheckCache();
}

void ExternalCacheImpl::OnDamagedFileDetected(const base::FilePath& path) {
  for (base::DictionaryValue::Iterator it(*cached_extensions_.get());
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* entry = nullptr;
    if (!it.value().GetAsDictionary(&entry)) {
      NOTREACHED() << "ExternalCacheImpl found bad entry with type "
                   << it.value().type();
      continue;
    }

    std::string external_crx;
    if (entry->GetString(extensions::ExternalProviderImpl::kExternalCrx,
                         &external_crx) &&
        external_crx == path.value()) {
      extensions::ExtensionId id = it.key();
      LOG(ERROR) << "ExternalCacheImpl extension at " << path.value()
                 << " failed to install, deleting it.";
      RemoveCachedExtension(id);
      UpdateExtensionLoader();

      // Don't try to DownloadMissingExtensions() from here,
      // since it can cause a fail/retry loop.
      // TODO(crbug.com/1121546) trigger re-installation mechanism with
      // exponential back-off.
      return;
    }
  }
  DLOG(ERROR) << "ExternalCacheImpl cannot find external_crx " << path.value();
}

void ExternalCacheImpl::RemoveExtensions(
    const std::vector<extensions::ExtensionId>& ids) {
  if (ids.empty())
    return;

  for (size_t i = 0; i < ids.size(); ++i) {
    extensions_->RemovePath(ids[i]);
    RemoveCachedExtension(ids[i]);
  }
  UpdateExtensionLoader();
}

void ExternalCacheImpl::RemoveCachedExtension(
    const extensions::ExtensionId& id) {
  cached_extensions_->RemovePath(id);
  local_cache_.RemoveExtension(id, std::string());

  if (delegate_)
    delegate_->OnCachedExtensionFileDeleted(id);
}

bool ExternalCacheImpl::GetExtension(const extensions::ExtensionId& id,
                                     base::FilePath* file_path,
                                     std::string* version) {
  return local_cache_.GetExtension(id, std::string(), file_path, version);
}

bool ExternalCacheImpl::ExtensionFetchPending(
    const extensions::ExtensionId& id) {
  return extensions_->HasKey(id) && !cached_extensions_->HasKey(id);
}

void ExternalCacheImpl::PutExternalExtension(
    const extensions::ExtensionId& id,
    const base::FilePath& crx_file_path,
    const std::string& version,
    PutExternalExtensionCallback callback) {
  local_cache_.PutExtension(
      id, std::string(), crx_file_path, version,
      base::BindOnce(&ExternalCacheImpl::OnPutExternalExtension,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void ExternalCacheImpl::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR, type);

  extensions::CrxInstaller* installer =
      content::Source<extensions::CrxInstaller>(source).ptr();
  OnDamagedFileDetected(installer->source_file());
}

void ExternalCacheImpl::OnExtensionDownloadFailed(
    const extensions::ExtensionId& id,
    Error error,
    const PingResult& ping_result,
    const std::set<int>& request_ids,
    const FailureData& data) {
  if (error == Error::NO_UPDATE_AVAILABLE) {
    if (!cached_extensions_->HasKey(id)) {
      LOG(ERROR) << "ExternalCacheImpl extension " << id
                 << " not found on update server";
      delegate_->OnExtensionDownloadFailed(id);
    } else {
      // No version update for an already cached extension.
      delegate_->OnExtensionLoadedInCache(id);
    }
  } else {
    LOG(ERROR) << "ExternalCacheImpl failed to download extension " << id
               << ", error " << static_cast<int>(error);
    delegate_->OnExtensionDownloadFailed(id);
  }
}

void ExternalCacheImpl::OnExtensionDownloadFinished(
    const extensions::CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const extensions::ExtensionDownloaderDelegate::PingResult& ping_result,
    const std::set<int>& request_ids,
    InstallCallback callback) {
  DCHECK(file_ownership_passed);
  DCHECK(file.expected_version.IsValid());
  local_cache_.PutExtension(
      file.extension_id, file.expected_hash, file.path,
      file.expected_version.GetString(),
      base::BindOnce(&ExternalCacheImpl::OnPutExtension,
                     weak_ptr_factory_.GetWeakPtr(), file.extension_id));
  if (!callback.is_null())
    std::move(callback).Run(true);
}

bool ExternalCacheImpl::IsExtensionPending(const extensions::ExtensionId& id) {
  return ExtensionFetchPending(id);
}

bool ExternalCacheImpl::GetExtensionExistingVersion(
    const extensions::ExtensionId& id,
    std::string* version) {
  base::DictionaryValue* extension_dictionary = nullptr;
  return cached_extensions_->GetDictionary(id, &extension_dictionary) &&
         extension_dictionary->GetString(
             extensions::ExternalProviderImpl::kExternalVersion, version);
}

void ExternalCacheImpl::UpdateExtensionLoader() {
  VLOG(1) << "Notify ExternalCacheImpl delegate about cache update";
  if (delegate_)
    delegate_->OnExtensionListsUpdated(cached_extensions_.get());
}

void ExternalCacheImpl::CheckCache() {
  if (local_cache_.is_shutdown())
    return;

  if (local_cache_.is_uninitialized()) {
    local_cache_.Init(wait_for_cache_initialization_,
                      base::BindOnce(&ExternalCacheImpl::CheckCache,
                                     weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If url_loader_factory_ is missing we can't download anything.
  if (url_loader_factory_) {
    downloader_ = ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
        url_loader_factory_, this, extensions::GetExternalVerifierFormat());
  }

  cached_extensions_->DictClear();
  for (const auto entry : extensions_->DictItems()) {
    if (!entry.second.is_dict()) {
      LOG(ERROR) << "ExternalCacheImpl found bad entry with type "
                 << entry.second.type();
      continue;
    }

    base::FilePath file_path;
    std::string version;
    std::string hash;
    bool is_cached =
        local_cache_.GetExtension(entry.first, hash, &file_path, &version);
    if (!is_cached)
      version = "0.0.0.0";
    if (downloader_) {
      GURL update_url =
          GetExtensionUpdateUrl(entry.second, always_check_updates_);

      if (update_url.is_valid()) {
        downloader_->AddPendingExtensionWithVersion(
            entry.first, update_url,
            extensions::mojom::ManifestLocation::kExternalPolicy, false, 0,
            extensions::ManifestFetchData::FetchPriority::BACKGROUND,
            base::Version(version), extensions::Manifest::TYPE_UNKNOWN,
            std::string());
      }
    }
    if (is_cached) {
      cached_extensions_->SetKey(
          entry.first,
          GetExtensionValueToCache(entry.second, file_path.value(), version));
    } else if (ShouldCacheImmediately(entry.second)) {
      cached_extensions_->SetKey(entry.first, entry.second.Clone());
    }
  }

  if (downloader_)
    downloader_->StartAllPending(nullptr);

  VLOG(1) << "Updated ExternalCacheImpl, there are "
          << cached_extensions_->DictSize() << " extensions cached";

  UpdateExtensionLoader();

  // Cancel already-scheduled check, if any. We want scheduled update to happen
  // when update interval passes after previous check, independent of what has
  // triggered this check.
  scheduler_weak_ptr_factory_.InvalidateWeakPtrs();

  MaybeScheduleNextCacheCheck();
}

void ExternalCacheImpl::MaybeScheduleNextCacheCheck() {
  if (!allow_scheduled_updates_) {
    return;
  }
  bool kiosk_crx_updates_from_policy = false;
  if (!(ash::CrosSettings::Get()->GetBoolean(
            ash::kKioskCRXManifestUpdateURLIgnored,
            &kiosk_crx_updates_from_policy) &&
        kiosk_crx_updates_from_policy)) {
    return;
  }

  // Jitter the frequency by +/- 20% like it's done in ExtensionUpdater.
  const double jitter_factor = base::RandDouble() * 0.4 + 0.8;
  base::TimeDelta delay =
      base::Seconds(extensions::kDefaultUpdateFrequencySeconds);
  delay *= jitter_factor;
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ExternalCacheImpl::CheckCache,
                         scheduler_weak_ptr_factory_.GetWeakPtr()),
          delay);
}

void ExternalCacheImpl::OnPutExtension(const extensions::ExtensionId& id,
                                       const base::FilePath& file_path,
                                       bool file_ownership_passed) {
  if (local_cache_.is_shutdown() || file_ownership_passed) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                       file_path));
    return;
  }

  VLOG(1) << "ExternalCacheImpl installed a new extension in the cache " << id;

  const base::Value* original_entry =
      extensions_->FindKeyOfType(id, base::Value::Type::DICTIONARY);
  if (!original_entry) {
    LOG(ERROR) << "ExternalCacheImpl cannot find entry for extension " << id;
    return;
  }

  std::string version;
  std::string hash;
  if (!local_cache_.GetExtension(id, hash, nullptr, &version)) {
    // Copy entry to don't modify it inside extensions_.
    LOG(ERROR) << "Can't find installed extension in cache " << id;
    return;
  }

  if (flush_on_put_) {
    backend_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(&FlushFile, file_path));
  }

  cached_extensions_->SetKey(
      id,
      GetExtensionValueToCache(*original_entry, file_path.value(), version));

  if (delegate_)
    delegate_->OnExtensionLoadedInCache(id);
  UpdateExtensionLoader();
}

void ExternalCacheImpl::OnPutExternalExtension(
    const extensions::ExtensionId& id,
    PutExternalExtensionCallback callback,
    const base::FilePath& file_path,
    bool file_ownership_passed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnPutExtension(id, file_path, file_ownership_passed);
  std::move(callback).Run(id, !file_ownership_passed);
}

}  // namespace chromeos
