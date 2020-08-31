// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/storage_handler.h"

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace protocol {

using ClearCookiesCallback = Storage::Backend::ClearCookiesCallback;
using GetCookiesCallback = Storage::Backend::GetCookiesCallback;
using SetCookiesCallback = Storage::Backend::SetCookiesCallback;

struct UsageListInitializer {
  const char* type;
  int64_t blink::mojom::UsageBreakdown::*usage_member;
};

UsageListInitializer initializers[] = {
    {Storage::StorageTypeEnum::File_systems,
     &blink::mojom::UsageBreakdown::fileSystem},
    {Storage::StorageTypeEnum::Websql, &blink::mojom::UsageBreakdown::webSql},
    {Storage::StorageTypeEnum::Appcache,
     &blink::mojom::UsageBreakdown::appcache},
    {Storage::StorageTypeEnum::Indexeddb,
     &blink::mojom::UsageBreakdown::indexedDatabase},
    {Storage::StorageTypeEnum::Cache_storage,
     &blink::mojom::UsageBreakdown::serviceWorkerCache},
    {Storage::StorageTypeEnum::Service_workers,
     &blink::mojom::UsageBreakdown::serviceWorker},
};

namespace {

void ReportUsageAndQuotaDataOnUIThread(
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback,
    blink::mojom::QuotaStatusCode code,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (code != blink::mojom::QuotaStatusCode::kOk) {
    return callback->sendFailure(
        Response::ServerError("Quota information is not available"));
  }

  auto usageList = std::make_unique<Array<Storage::UsageForType>>();

  blink::mojom::UsageBreakdown* breakdown_ptr = usage_breakdown.get();
  for (const auto initializer : initializers) {
    std::unique_ptr<Storage::UsageForType> entry =
        Storage::UsageForType::Create()
            .SetStorageType(initializer.type)
            .SetUsage(breakdown_ptr->*(initializer.usage_member))
            .Build();
    usageList->emplace_back(std::move(entry));
  }

  callback->sendSuccess(usage, quota, std::move(usageList));
}

void GotUsageAndQuotaDataCallback(
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback,
    blink::mojom::QuotaStatusCode code,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(ReportUsageAndQuotaDataOnUIThread, std::move(callback),
                     code, usage, quota, std::move(usage_breakdown)));
}

void GetUsageAndQuotaOnIOThread(
    storage::QuotaManager* manager,
    const url::Origin& origin,
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  manager->GetUsageAndQuotaWithBreakdown(
      origin, blink::mojom::StorageType::kTemporary,
      base::BindOnce(&GotUsageAndQuotaDataCallback, std::move(callback)));
}

}  // namespace

// Observer that listens on the IO thread for cache storage notifications and
// informs the StorageHandler on the UI thread for origins of interest.
// Created on the UI thread but predominantly used and deleted on the IO thread.
// Registered on creation as an observer in CacheStorageContextImpl,
// unregistered on destruction.
class StorageHandler::CacheStorageObserver : CacheStorageContextImpl::Observer {
 public:
  CacheStorageObserver(base::WeakPtr<StorageHandler> owner_storage_handler,
                       CacheStorageContextImpl* cache_storage_context)
      : owner_(owner_storage_handler), context_(cache_storage_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    context_->AddObserver(this);
  }

  ~CacheStorageObserver() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    context_->RemoveObserver(this);
  }

  void TrackOrigin(const url::Origin& origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (origins_.find(origin) != origins_.end())
      return;
    origins_.insert(origin);
  }

  void UntrackOrigin(const url::Origin& origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    origins_.erase(origin);
  }

  void OnCacheListChanged(const url::Origin& origin) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto found = origins_.find(origin);
    if (found == origins_.end())
      return;
    owner_->NotifyCacheStorageListChanged(origin.Serialize());
  }

  void OnCacheContentChanged(const url::Origin& origin,
                             const std::string& cache_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (origins_.find(origin) == origins_.end())
      return;
    owner_->NotifyCacheStorageContentChanged(origin.Serialize(), cache_name);
  }

 private:
  // Maintained on the IO thread to avoid thread contention.
  base::flat_set<url::Origin> origins_;

  base::WeakPtr<StorageHandler> owner_;
  scoped_refptr<CacheStorageContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageObserver);
};

// Observer that listens on the IDB thread for IndexedDB notifications and
// informs the StorageHandler on the UI thread for origins of interest.
// Created on the UI thread but predominantly used and deleted on the IDB
// thread.
class StorageHandler::IndexedDBObserver
    : public storage::mojom::IndexedDBObserver {
 public:
  explicit IndexedDBObserver(
      base::WeakPtr<StorageHandler> owner_storage_handler)
      : owner_(owner_storage_handler), receiver_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    ReconnectObserver();
  }

  ~IndexedDBObserver() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  void TrackOrigin(const url::Origin& origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (origins_.find(origin) != origins_.end())
      return;
    origins_.insert(origin);
  }

  void UntrackOrigin(const url::Origin& origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    origins_.erase(origin);
  }

  void OnIndexedDBListChanged(const url::Origin& origin) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!owner_)
      return;
    auto found = origins_.find(origin);
    if (found == origins_.end())
      return;
    owner_->NotifyIndexedDBListChanged(origin.Serialize());
  }

  void OnIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!owner_)
      return;
    auto found = origins_.find(origin);
    if (found == origins_.end())
      return;
    owner_->NotifyIndexedDBContentChanged(origin.Serialize(), database_name,
                                          object_store_name);
  }

 private:
  void ReconnectObserver() {
    DCHECK(!receiver_.is_bound());
    if (!owner_)
      return;

    auto& control = owner_->storage_partition_->GetIndexedDBControl();
    mojo::PendingRemote<storage::mojom::IndexedDBObserver> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(base::BindOnce(
        [](IndexedDBObserver* observer) {
          // If this observer disconnects because IndexedDB or the storage
          // service goes away, reconnect again.
          observer->ReconnectObserver();
        },
        this));
    control.AddObserver(std::move(remote));
  }

  base::flat_set<url::Origin> origins_;
  base::WeakPtr<StorageHandler> owner_;
  mojo::Receiver<storage::mojom::IndexedDBObserver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBObserver);
};

StorageHandler::StorageHandler()
    : DevToolsDomainHandler(Storage::Metainfo::domainName),
      storage_partition_(nullptr) {}

StorageHandler::~StorageHandler() {
  DCHECK(!cache_storage_observer_);
  DCHECK(!indexed_db_observer_);
}

void StorageHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Storage::Frontend>(dispatcher->channel());
  Storage::Dispatcher::wire(dispatcher, this);
}

void StorageHandler::SetRenderer(int process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_host_id);
  storage_partition_ = process ? process->GetStoragePartition() : nullptr;
}

Response StorageHandler::Disable() {
  cache_storage_observer_.reset();
  indexed_db_observer_.reset();
  return Response::Success();
}

void StorageHandler::GetCookies(Maybe<std::string> browser_context_id,
                                std::unique_ptr<GetCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  storage_partition->GetCookieManagerForBrowserProcess()->GetAllCookies(
      base::BindOnce(
          [](std::unique_ptr<GetCookiesCallback> callback,
             const std::vector<net::CanonicalCookie>& cookies) {
            callback->sendSuccess(NetworkHandler::BuildCookieArray(cookies));
          },
          std::move(callback)));
}

void StorageHandler::SetCookies(
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    Maybe<std::string> browser_context_id,
    std::unique_ptr<SetCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  NetworkHandler::SetCookies(
      storage_partition, std::move(cookies),
      base::BindOnce(
          [](std::unique_ptr<SetCookiesCallback> callback, bool success) {
            if (success) {
              callback->sendSuccess();
            } else {
              callback->sendFailure(
                  Response::InvalidParams("Invalid cookie fields"));
            }
          },
          std::move(callback)));
}

void StorageHandler::ClearCookies(
    Maybe<std::string> browser_context_id,
    std::unique_ptr<ClearCookiesCallback> callback) {
  StoragePartition* storage_partition = nullptr;
  Response response = StorageHandler::FindStoragePartition(browser_context_id,
                                                           &storage_partition);
  if (!response.IsSuccess()) {
    callback->sendFailure(std::move(response));
    return;
  }

  storage_partition->GetCookieManagerForBrowserProcess()->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(),
      base::BindOnce([](std::unique_ptr<ClearCookiesCallback> callback,
                        uint32_t) { callback->sendSuccess(); },
                     std::move(callback)));
}

void StorageHandler::ClearDataForOrigin(
    const std::string& origin,
    const std::string& storage_types,
    std::unique_ptr<ClearDataForOriginCallback> callback) {
  if (!storage_partition_)
    return callback->sendFailure(Response::InternalError());

  std::vector<std::string> types = base::SplitString(
      storage_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> set(types.begin(), types.end());
  uint32_t remove_mask = 0;
  if (set.count(Storage::StorageTypeEnum::Appcache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  if (set.count(Storage::StorageTypeEnum::Cookies))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  if (set.count(Storage::StorageTypeEnum::File_systems))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (set.count(Storage::StorageTypeEnum::Indexeddb))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  if (set.count(Storage::StorageTypeEnum::Local_storage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (set.count(Storage::StorageTypeEnum::Shader_cache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  if (set.count(Storage::StorageTypeEnum::Websql))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  if (set.count(Storage::StorageTypeEnum::Service_workers))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  if (set.count(Storage::StorageTypeEnum::Cache_storage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  if (set.count(Storage::StorageTypeEnum::All))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_ALL;

  if (!remove_mask) {
    return callback->sendFailure(
        Response::InvalidParams("No valid storage type specified"));
  }

  storage_partition_->ClearData(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(origin), base::Time(), base::Time::Max(),
      base::BindOnce(&ClearDataForOriginCallback::sendSuccess,
                     std::move(callback)));
}

void StorageHandler::GetUsageAndQuota(
    const String& origin,
    std::unique_ptr<GetUsageAndQuotaCallback> callback) {
  if (!storage_partition_)
    return callback->sendFailure(Response::InternalError());

  GURL origin_url(origin);
  if (!origin_url.is_valid()) {
    return callback->sendFailure(
        Response::ServerError(origin + " is not a valid URL"));
  }

  storage::QuotaManager* manager = storage_partition_->GetQuotaManager();
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetUsageAndQuotaOnIOThread, base::RetainedRef(manager),
                     url::Origin::Create(origin_url), std::move(callback)));
}

Response StorageHandler::TrackCacheStorageForOrigin(const std::string& origin) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin);
  if (!origin_url.is_valid())
    return Response::InvalidParams(origin + " is not a valid URL");

  GetCacheStorageObserver()->TrackOrigin(url::Origin::Create(origin_url));
  return Response::Success();
}

Response StorageHandler::UntrackCacheStorageForOrigin(
    const std::string& origin) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin);
  if (!origin_url.is_valid())
    return Response::InvalidParams(origin + " is not a valid URL");

  GetCacheStorageObserver()->UntrackOrigin(url::Origin::Create(origin_url));
  return Response::Success();
}

Response StorageHandler::TrackIndexedDBForOrigin(const std::string& origin) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin);
  if (!origin_url.is_valid())
    return Response::InvalidParams(origin + " is not a valid URL");

  GetIndexedDBObserver()->TrackOrigin(url::Origin::Create(origin_url));
  return Response::Success();
}

Response StorageHandler::UntrackIndexedDBForOrigin(const std::string& origin) {
  if (!storage_partition_)
    return Response::InternalError();

  GURL origin_url(origin);
  if (!origin_url.is_valid())
    return Response::InvalidParams(origin + " is not a valid URL");

  GetIndexedDBObserver()->UntrackOrigin(url::Origin::Create(origin_url));
  return Response::Success();
}

StorageHandler::CacheStorageObserver*
StorageHandler::GetCacheStorageObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!cache_storage_observer_) {
    cache_storage_observer_ = std::make_unique<CacheStorageObserver>(
        weak_ptr_factory_.GetWeakPtr(),
        static_cast<CacheStorageContextImpl*>(
            storage_partition_->GetCacheStorageContext()));
  }
  return cache_storage_observer_.get();
}

StorageHandler::IndexedDBObserver* StorageHandler::GetIndexedDBObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!indexed_db_observer_) {
    indexed_db_observer_ =
        std::make_unique<IndexedDBObserver>(weak_ptr_factory_.GetWeakPtr());
  }
  return indexed_db_observer_.get();
}

void StorageHandler::NotifyCacheStorageListChanged(const std::string& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->CacheStorageListUpdated(origin);
}

void StorageHandler::NotifyCacheStorageContentChanged(const std::string& origin,
                                                      const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->CacheStorageContentUpdated(origin, name);
}

void StorageHandler::NotifyIndexedDBListChanged(const std::string& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->IndexedDBListUpdated(origin);
}

void StorageHandler::NotifyIndexedDBContentChanged(
    const std::string& origin,
    const base::string16& database_name,
    const base::string16& object_store_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frontend_->IndexedDBContentUpdated(origin, base::UTF16ToUTF8(database_name),
                                     base::UTF16ToUTF8(object_store_name));
}

Response StorageHandler::FindStoragePartition(
    const Maybe<std::string>& browser_context_id,
    StoragePartition** storage_partition) {
  BrowserContext* browser_context = nullptr;
  Response response =
      BrowserHandler::FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;
  *storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  if (!*storage_partition)
    return Response::InternalError();
  return Response::Success();
}

}  // namespace protocol
}  // namespace content
