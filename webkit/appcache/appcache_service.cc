// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_histograms.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_quota_client.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage_impl.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

namespace appcache {

AppCacheInfoCollection::AppCacheInfoCollection() {}

AppCacheInfoCollection::~AppCacheInfoCollection() {}

// AsyncHelper -------

class AppCacheService::NewAsyncHelper
    : public AppCacheStorage::Delegate {
 public:
  NewAsyncHelper(AppCacheService* service,
                 const net::CompletionCallback& callback)
      : service_(service), callback_(callback) {
    service_->pending_new_helpers_.insert(this);
  }

  virtual ~NewAsyncHelper() {
    if (service_)
      service_->pending_new_helpers_.erase(this);
  }

  virtual void Start() = 0;
  virtual void Cancel();

 protected:
  void CallCallback(int rv) {
    if (!callback_.is_null()) {
      // Defer to guarantee async completion.
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&DeferredCallCallback, callback_, rv));
    }
    callback_.Reset();
  }

  static void DeferredCallCallback(const net::CompletionCallback& callback,
                                   int rv) {
    callback.Run(rv);
  }

  AppCacheService* service_;
  net::CompletionCallback callback_;
};

void AppCacheService::NewAsyncHelper::Cancel() {
  if (!callback_.is_null()) {
    callback_.Run(net::ERR_ABORTED);
    callback_.Reset();
  }

  service_->storage()->CancelDelegateCallbacks(this);
  service_ = NULL;
}

class AppCacheService::AsyncHelper
    : public AppCacheStorage::Delegate {
 public:
  AsyncHelper(
      AppCacheService* service, net::OldCompletionCallback* callback)
      : service_(service), callback_(callback) {
    service_->pending_helpers_.insert(this);
  }

  virtual ~AsyncHelper() {
    if (service_)
      service_->pending_helpers_.erase(this);
  }

  virtual void Start() = 0;
  virtual void Cancel();

 protected:
  void CallCallback(int rv) {
    if (callback_) {
      // Defer to guarantee async completion.
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&DeferredCallCallback, callback_, rv));
    }
    callback_ = NULL;
  }

  static void DeferredCallCallback(net::OldCompletionCallback* callback,
                                   int rv) {
    callback->Run(rv);
  }

  AppCacheService* service_;
  net::OldCompletionCallback* callback_;
};

void AppCacheService::AsyncHelper::Cancel() {
  if (callback_) {
    callback_->Run(net::ERR_ABORTED);
    callback_ = NULL;
  }
  service_->storage()->CancelDelegateCallbacks(this);
  service_ = NULL;
}

// CanHandleOfflineHelper -------

class AppCacheService::CanHandleOfflineHelper : NewAsyncHelper {
 public:
  CanHandleOfflineHelper(
      AppCacheService* service, const GURL& url,
      const GURL& first_party, const net::CompletionCallback& callback)
      : NewAsyncHelper(service, callback),
        url_(url),
        first_party_(first_party) {
  }

  virtual void Start() {
    AppCachePolicy* policy = service_->appcache_policy();
    if (policy && !policy->CanLoadAppCache(url_, first_party_)) {
      CallCallback(net::ERR_FAILED);
      delete this;
      return;
    }

    service_->storage()->FindResponseForMainRequest(url_, GURL(), this);
  }

 private:
  // AppCacheStorage::Delegate implementation.
  virtual void OnMainResponseFound(
      const GURL& url, const AppCacheEntry& entry,
      const GURL& fallback_url, const AppCacheEntry& fallback_entry,
      int64 cache_id, int64 group_id, const GURL& mainfest_url) OVERRIDE;

  GURL url_;
  GURL first_party_;

  DISALLOW_COPY_AND_ASSIGN(CanHandleOfflineHelper);
};

void AppCacheService::CanHandleOfflineHelper::OnMainResponseFound(
      const GURL& url, const AppCacheEntry& entry,
      const GURL& fallback_url, const AppCacheEntry& fallback_entry,
      int64 cache_id, int64 group_id, const GURL& manifest_url) {
  bool can = (entry.has_response_id() || fallback_entry.has_response_id());
  CallCallback(can ? net::OK : net::ERR_FAILED);
  delete this;
}

// DeleteHelper -------

class AppCacheService::DeleteHelper : public NewAsyncHelper {
 public:
  DeleteHelper(
      AppCacheService* service, const GURL& manifest_url,
      const net::CompletionCallback& callback)
      : NewAsyncHelper(service, callback), manifest_url_(manifest_url) {
  }

  virtual void Start() {
    service_->storage()->LoadOrCreateGroup(manifest_url_, this);
  }

 private:
  // AppCacheStorage::Delegate implementation.
  virtual void OnGroupLoaded(
      appcache::AppCacheGroup* group, const GURL& manifest_url);
  virtual void OnGroupMadeObsolete(
      appcache::AppCacheGroup* group, bool success);

  GURL manifest_url_;
  DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
};

void AppCacheService::DeleteHelper::OnGroupLoaded(
      appcache::AppCacheGroup* group, const GURL& manifest_url) {
  if (group) {
    group->set_being_deleted(true);
    group->CancelUpdate();
    service_->storage()->MakeGroupObsolete(group, this);
  } else {
    CallCallback(net::ERR_FAILED);
    delete this;
  }
}

void AppCacheService::DeleteHelper::OnGroupMadeObsolete(
      appcache::AppCacheGroup* group, bool success) {
  CallCallback(success ? net::OK : net::ERR_FAILED);
  delete this;
}

// DeleteOriginHelper -------

class AppCacheService::DeleteOriginHelper : public NewAsyncHelper {
 public:
  DeleteOriginHelper(
      AppCacheService* service, const GURL& origin,
      const net::CompletionCallback& callback)
      : NewAsyncHelper(service, callback), origin_(origin),
        num_caches_to_delete_(0), successes_(0), failures_(0) {
  }

  virtual void Start() {
    // We start by listing all caches, continues in OnAllInfo().
    service_->storage()->GetAllInfo(this);
  }

 private:
  // AppCacheStorage::Delegate implementation.
  virtual void OnAllInfo(AppCacheInfoCollection* collection);
  virtual void OnGroupLoaded(
      appcache::AppCacheGroup* group, const GURL& manifest_url);
  virtual void OnGroupMadeObsolete(
      appcache::AppCacheGroup* group, bool success);

  void CacheCompleted(bool success);

  GURL origin_;
  int num_caches_to_delete_;
  int successes_;
  int failures_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOriginHelper);
};

void AppCacheService::DeleteOriginHelper::OnAllInfo(
    AppCacheInfoCollection* collection) {
  if (!collection) {
    // Failed to get a listing.
    CallCallback(net::ERR_FAILED);
    delete this;
    return;
  }

  std::map<GURL, AppCacheInfoVector>::iterator found =
      collection->infos_by_origin.find(origin_);
  if (found == collection->infos_by_origin.end() || found->second.empty()) {
    // No caches for this origin.
    CallCallback(net::OK);
    delete this;
    return;
  }

  // We have some caches to delete.
  const AppCacheInfoVector& caches_to_delete = found->second;
  successes_ = 0;
  failures_ = 0;
  num_caches_to_delete_ = static_cast<int>(caches_to_delete.size());
  for (AppCacheInfoVector::const_iterator iter = caches_to_delete.begin();
       iter != caches_to_delete.end(); ++iter) {
    service_->storage()->LoadOrCreateGroup(iter->manifest_url, this);
  }
}

void AppCacheService::DeleteOriginHelper::OnGroupLoaded(
      appcache::AppCacheGroup* group, const GURL& manifest_url) {
  if (group) {
    group->set_being_deleted(true);
    group->CancelUpdate();
    service_->storage()->MakeGroupObsolete(group, this);
  } else {
    CacheCompleted(false);
  }
}

void AppCacheService::DeleteOriginHelper::OnGroupMadeObsolete(
      appcache::AppCacheGroup* group, bool success) {
  CacheCompleted(success);
}

void AppCacheService::DeleteOriginHelper::CacheCompleted(bool success) {
  if (success)
    ++successes_;
  else
    ++failures_;
  if ((successes_ + failures_) < num_caches_to_delete_)
    return;

  CallCallback(!failures_ ? net::OK : net::ERR_FAILED);
  delete this;
}


// GetInfoHelper -------

class AppCacheService::GetInfoHelper : NewAsyncHelper {
 public:
  GetInfoHelper(
      AppCacheService* service, AppCacheInfoCollection* collection,
      const net::CompletionCallback& callback)
      : NewAsyncHelper(service, callback), collection_(collection) {
  }

  virtual void Start() {
    service_->storage()->GetAllInfo(this);
  }

 private:
  // AppCacheStorage::Delegate implementation.
  virtual void OnAllInfo(AppCacheInfoCollection* collection);

  scoped_refptr<AppCacheInfoCollection> collection_;

  DISALLOW_COPY_AND_ASSIGN(GetInfoHelper);
};

void AppCacheService::GetInfoHelper::OnAllInfo(
      AppCacheInfoCollection* collection) {
  if (collection)
    collection->infos_by_origin.swap(collection_->infos_by_origin);
  CallCallback(collection ? net::OK : net::ERR_FAILED);
  delete this;
}

// CheckResponseHelper -------

class AppCacheService::CheckResponseHelper : AsyncHelper {
 public:
  CheckResponseHelper(
      AppCacheService* service, const GURL& manifest_url, int64 cache_id,
      int64 response_id)
      : AsyncHelper(service, NULL),
        manifest_url_(manifest_url),
        cache_id_(cache_id),
        response_id_(response_id),
        kIOBufferSize(32 * 1024),
        expected_total_size_(0),
        amount_headers_read_(0),
        amount_data_read_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(read_info_callback_(
            this, &CheckResponseHelper::OnReadInfoComplete)),
        ALLOW_THIS_IN_INITIALIZER_LIST(read_data_callback_(
            this, &CheckResponseHelper::OnReadDataComplete)) {
  }

  virtual void Start() {
    service_->storage()->LoadOrCreateGroup(manifest_url_, this);
  }

  virtual void Cancel() {
    AppCacheHistograms::CountCheckResponseResult(
        AppCacheHistograms::CHECK_CANCELED);
    response_reader_.reset();
    AsyncHelper::Cancel();
  }

 private:
  virtual void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url);
  void OnReadInfoComplete(int result);
  void OnReadDataComplete(int result);

  // Inputs describing what to check.
  GURL manifest_url_;
  int64 cache_id_;
  int64 response_id_;

  // Internals used to perform the checks.
  const int kIOBufferSize;
  scoped_refptr<AppCache> cache_;
  scoped_ptr<AppCacheResponseReader> response_reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  scoped_refptr<net::IOBuffer> data_buffer_;
  int64 expected_total_size_;
  int amount_headers_read_;
  int amount_data_read_;
  net::OldCompletionCallbackImpl<CheckResponseHelper> read_info_callback_;
  net::OldCompletionCallbackImpl<CheckResponseHelper> read_data_callback_;
  DISALLOW_COPY_AND_ASSIGN(CheckResponseHelper);
};

void AppCacheService::CheckResponseHelper::OnGroupLoaded(
    AppCacheGroup* group, const GURL& manifest_url) {
  DCHECK_EQ(manifest_url_, manifest_url);
  if (!group || !group->newest_complete_cache() || group->is_being_deleted() ||
      group->is_obsolete()) {
    AppCacheHistograms::CountCheckResponseResult(
        AppCacheHistograms::MANIFEST_OUT_OF_DATE);
    delete this;
    return;
  }

  cache_ = group->newest_complete_cache();
  const AppCacheEntry* entry = cache_->GetEntryWithResponseId(response_id_);
  if (!entry) {
    if (cache_->cache_id() == cache_id_) {
      AppCacheHistograms::CountCheckResponseResult(
          AppCacheHistograms::ENTRY_NOT_FOUND);
      service_->DeleteAppCacheGroup(manifest_url_, net::CompletionCallback());
    } else {
      AppCacheHistograms::CountCheckResponseResult(
          AppCacheHistograms::RESPONSE_OUT_OF_DATE);
    }
    delete this;
    return;
  }

  // Verify that we can read the response info and data.
  expected_total_size_ = entry->response_size();
  response_reader_.reset(service_->storage()->CreateResponseReader(
      manifest_url_, group->group_id(), response_id_));
  info_buffer_ = new HttpResponseInfoIOBuffer();
  response_reader_->ReadInfo(info_buffer_, &read_info_callback_);
}

void AppCacheService::CheckResponseHelper::OnReadInfoComplete(int result) {
  if (result < 0) {
    AppCacheHistograms::CountCheckResponseResult(
        AppCacheHistograms::READ_HEADERS_ERROR);
    service_->DeleteAppCacheGroup(manifest_url_, net::CompletionCallback());
    delete this;
    return;
  }
  amount_headers_read_ = result;

  // Start reading the data.
  data_buffer_ = new net::IOBuffer(kIOBufferSize);
  response_reader_->ReadData(data_buffer_, kIOBufferSize,
                             &read_data_callback_);
}

void AppCacheService::CheckResponseHelper::OnReadDataComplete(int result) {
  if (result > 0) {
    // Keep reading until we've read thru everything or failed to read.
    amount_data_read_ += result;
    response_reader_->ReadData(data_buffer_, kIOBufferSize,
                               &read_data_callback_);
    return;
  }

  AppCacheHistograms::CheckResponseResultType check_result;
  if (result < 0)
    check_result = AppCacheHistograms::READ_DATA_ERROR;
  else if (info_buffer_->response_data_size != amount_data_read_ ||
           expected_total_size_ != amount_data_read_ + amount_headers_read_)
    check_result = AppCacheHistograms::UNEXPECTED_DATA_SIZE;
  else
    check_result = AppCacheHistograms::RESPONSE_OK;
  AppCacheHistograms::CountCheckResponseResult(check_result);

  if (check_result != AppCacheHistograms::RESPONSE_OK)
    service_->DeleteAppCacheGroup(manifest_url_, net::CompletionCallback());
  delete this;
}


// AppCacheService -------

AppCacheService::AppCacheService(quota::QuotaManagerProxy* quota_manager_proxy)
    : appcache_policy_(NULL), quota_client_(NULL),
      quota_manager_proxy_(quota_manager_proxy),
      request_context_(NULL), clear_local_state_on_exit_(false)  {
  if (quota_manager_proxy_) {
    quota_client_ = new AppCacheQuotaClient(this);
    quota_manager_proxy_->RegisterClient(quota_client_);
  }
}

AppCacheService::~AppCacheService() {
  DCHECK(backends_.empty());
  std::for_each(pending_helpers_.begin(),
                pending_helpers_.end(),
                std::mem_fun(&AsyncHelper::Cancel));
  STLDeleteElements(&pending_helpers_);
  std::for_each(pending_new_helpers_.begin(),
                pending_new_helpers_.end(),
                std::mem_fun(&NewAsyncHelper::Cancel));
  STLDeleteElements(&pending_new_helpers_);
  if (quota_client_)
    quota_client_->NotifyAppCacheDestroyed();

  // Destroy storage_ first; ~AppCacheStorageImpl accesses other data members
  // (special_storage_policy_).
  storage_.reset();
}

void AppCacheService::Initialize(const FilePath& cache_directory,
                                 base::MessageLoopProxy* db_thread,
                                 base::MessageLoopProxy* cache_thread) {
  DCHECK(!storage_.get());
  AppCacheStorageImpl* storage = new AppCacheStorageImpl(this);
  storage->Initialize(cache_directory, db_thread, cache_thread);
  storage_.reset(storage);
}

void AppCacheService::CanHandleMainResourceOffline(
    const GURL& url,
    const GURL& first_party,
    const net::CompletionCallback& callback) {
  CanHandleOfflineHelper* helper =
      new CanHandleOfflineHelper(this, url, first_party, callback);
  helper->Start();
}

void AppCacheService::GetAllAppCacheInfo(
    AppCacheInfoCollection* collection,
    const net::CompletionCallback& callback) {
  DCHECK(collection);
  GetInfoHelper* helper = new GetInfoHelper(this, collection, callback);
  helper->Start();
}

void AppCacheService::DeleteAppCacheGroup(
    const GURL& manifest_url,
    const net::CompletionCallback& callback) {
  DeleteHelper* helper = new DeleteHelper(this, manifest_url, callback);
  helper->Start();
}

void AppCacheService::DeleteAppCachesForOrigin(
    const GURL& origin,  const net::CompletionCallback& callback) {
  DeleteOriginHelper* helper = new DeleteOriginHelper(this, origin, callback);
  helper->Start();
}

void AppCacheService::CheckAppCacheResponse(const GURL& manifest_url,
                                            int64 cache_id,
                                            int64 response_id) {
  CheckResponseHelper* helper = new CheckResponseHelper(
      this, manifest_url, cache_id, response_id);
  helper->Start();
}

void AppCacheService::set_special_storage_policy(
    quota::SpecialStoragePolicy* policy) {
  special_storage_policy_ = policy;
}

void AppCacheService::RegisterBackend(
    AppCacheBackendImpl* backend_impl) {
  DCHECK(backends_.find(backend_impl->process_id()) == backends_.end());
  backends_.insert(
      BackendMap::value_type(backend_impl->process_id(), backend_impl));
}

void AppCacheService::UnregisterBackend(
    AppCacheBackendImpl* backend_impl) {
  backends_.erase(backend_impl->process_id());
}

}  // namespace appcache
