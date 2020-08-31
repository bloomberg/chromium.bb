// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
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

// Wraps a base::OnceCallback with a base::Callback, which can be passed as a
// callback to extensions::LocalExtensionCache::PutExtension().
// TODO(tbarzic): Remove this when LocalExtensionCache starts using
//     OnceCallback.
void WrapPutExtensionCallback(
    base::OnceCallback<void(const base::FilePath&, bool)> callback,
    const base::FilePath& file_path,
    bool file_ownership_passed) {
  if (callback)
    std::move(callback).Run(file_path, file_ownership_passed);
}

// Wraps OnceClosure with a base::Closure, so it can be passed to
// extensions::LocalExtensionCache::Shutdown.
// TODO(tbarzic): Remove this when LocakExtensionCache starts using OnceClosure.
void WrapOnceClosure(base::OnceClosure closure) {
  if (closure)
    std::move(closure).Run();
}

}  // namespace

ExternalCacheImpl::ExternalCacheImpl(
    const base::FilePath& cache_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    ExternalCacheDelegate* delegate,
    bool always_check_updates,
    bool wait_for_cache_initialization)
    : local_cache_(cache_dir, 0, base::TimeDelta(), backend_task_runner),
      url_loader_factory_(std::move(url_loader_factory)),
      backend_task_runner_(backend_task_runner),
      delegate_(delegate),
      always_check_updates_(always_check_updates),
      wait_for_cache_initialization_(wait_for_cache_initialization),
      cached_extensions_(new base::DictionaryValue()) {
  notification_registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
}

ExternalCacheImpl::~ExternalCacheImpl() = default;

const base::DictionaryValue* ExternalCacheImpl::GetCachedExtensions() {
  return cached_extensions_.get();
}

void ExternalCacheImpl::Shutdown(base::OnceClosure callback) {
  local_cache_.Shutdown(
      base::Bind(&WrapOnceClosure, base::Passed(std::move(callback))));
}

void ExternalCacheImpl::UpdateExtensionsList(
    std::unique_ptr<base::DictionaryValue> prefs) {
  extensions_ = std::move(prefs);

  if (extensions_->empty()) {
    // If list of know extensions is empty, don't init cache on disk. It is
    // important shortcut for test to don't wait forever for cache dir
    // initialization that should happen outside of Chrome on real device.
    cached_extensions_->Clear();
    UpdateExtensionLoader();
    return;
  }

  if (local_cache_.is_uninitialized()) {
    local_cache_.Init(wait_for_cache_initialization_,
                      base::Bind(&ExternalCacheImpl::CheckCache,
                                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    CheckCache();
  }
}

void ExternalCacheImpl::OnDamagedFileDetected(const base::FilePath& path) {
  for (base::DictionaryValue::Iterator it(*cached_extensions_.get());
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* entry = NULL;
    if (!it.value().GetAsDictionary(&entry)) {
      NOTREACHED() << "ExternalCacheImpl found bad entry with type "
                   << it.value().type();
      continue;
    }

    std::string external_crx;
    if (entry->GetString(extensions::ExternalProviderImpl::kExternalCrx,
                         &external_crx) &&
        external_crx == path.value()) {
      std::string id = it.key();
      LOG(ERROR) << "ExternalCacheImpl extension at " << path.value()
                 << " failed to install, deleting it.";
      cached_extensions_->Remove(id, NULL);
      extensions_->Remove(id, NULL);

      local_cache_.RemoveExtension(id, std::string());
      UpdateExtensionLoader();

      // Don't try to DownloadMissingExtensions() from here,
      // since it can cause a fail/retry loop.
      return;
    }
  }
  DLOG(ERROR) << "ExternalCacheImpl cannot find external_crx " << path.value();
}

void ExternalCacheImpl::RemoveExtensions(const std::vector<std::string>& ids) {
  if (ids.empty())
    return;

  for (size_t i = 0; i < ids.size(); ++i) {
    cached_extensions_->Remove(ids[i], NULL);
    extensions_->Remove(ids[i], NULL);
    local_cache_.RemoveExtension(ids[i], std::string());
  }
  UpdateExtensionLoader();
}

bool ExternalCacheImpl::GetExtension(const std::string& id,
                                     base::FilePath* file_path,
                                     std::string* version) {
  return local_cache_.GetExtension(id, std::string(), file_path, version);
}

bool ExternalCacheImpl::ExtensionFetchPending(const std::string& id) {
  return extensions_->HasKey(id) && !cached_extensions_->HasKey(id);
}

void ExternalCacheImpl::PutExternalExtension(
    const std::string& id,
    const base::FilePath& crx_file_path,
    const std::string& version,
    PutExternalExtensionCallback callback) {
  local_cache_.PutExtension(
      id, std::string(), crx_file_path, version,
      base::Bind(
          &WrapPutExtensionCallback,
          base::Passed(base::BindOnce(
              &ExternalCacheImpl::OnPutExternalExtension,
              weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)))));
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
    const std::string& id,
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
    const InstallCallback& callback) {
  DCHECK(file_ownership_passed);
  local_cache_.PutExtension(
      file.extension_id, file.expected_hash, file.path, file.expected_version,
      base::Bind(&ExternalCacheImpl::OnPutExtension,
                 weak_ptr_factory_.GetWeakPtr(), file.extension_id));
  if (!callback.is_null())
    callback.Run(true);
}

bool ExternalCacheImpl::IsExtensionPending(const std::string& id) {
  return ExtensionFetchPending(id);
}

bool ExternalCacheImpl::GetExtensionExistingVersion(const std::string& id,
                                                    std::string* version) {
  base::DictionaryValue* extension_dictionary = NULL;
  if (cached_extensions_->GetDictionary(id, &extension_dictionary)) {
    if (extension_dictionary->GetString(
            extensions::ExternalProviderImpl::kExternalVersion, version)) {
      return true;
    }
    *version = delegate_->GetInstalledExtensionVersion(id);
    return !version->empty();
  }
  return false;
}

void ExternalCacheImpl::UpdateExtensionLoader() {
  VLOG(1) << "Notify ExternalCacheImpl delegate about cache update";
  if (delegate_)
    delegate_->OnExtensionListsUpdated(cached_extensions_.get());
}

void ExternalCacheImpl::CheckCache() {
  if (local_cache_.is_shutdown())
    return;

  // If url_loader_factory_ is missing we can't download anything.
  if (url_loader_factory_) {
    downloader_ = ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
        url_loader_factory_, this, extensions::GetExternalVerifierFormat());
  }

  cached_extensions_->Clear();
  for (const auto& entry : extensions_->DictItems()) {
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
            entry.first, update_url, extensions::Manifest::EXTERNAL_POLICY,
            false, 0, extensions::ManifestFetchData::FetchPriority::BACKGROUND,
            base::Version(version));
      }
    }
    if (is_cached) {
      cached_extensions_->SetKey(
          entry.first,
          GetExtensionValueToCache(entry.second, file_path.value(), version));
    } else if (ShouldCacheImmediately(
                   entry.second,
                   delegate_->GetInstalledExtensionVersion(entry.first))) {
      cached_extensions_->SetKey(entry.first, entry.second.Clone());
    }
  }

  if (downloader_)
    downloader_->StartAllPending(NULL);

  VLOG(1) << "Updated ExternalCacheImpl, there are "
          << cached_extensions_->size() << " extensions cached";

  UpdateExtensionLoader();
}

void ExternalCacheImpl::OnPutExtension(const std::string& id,
                                       const base::FilePath& file_path,
                                       bool file_ownership_passed) {
  if (local_cache_.is_shutdown() || file_ownership_passed) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeleteFileRecursively),
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
  if (!local_cache_.GetExtension(id, hash, NULL, &version)) {
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
    const std::string& id,
    PutExternalExtensionCallback callback,
    const base::FilePath& file_path,
    bool file_ownership_passed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnPutExtension(id, file_path, file_ownership_passed);
  std::move(callback).Run(id, !file_ownership_passed);
}

}  // namespace chromeos
