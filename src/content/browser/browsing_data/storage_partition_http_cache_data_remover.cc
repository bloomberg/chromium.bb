// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/storage_partition_http_cache_data_remover.h"

#include "base/location.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browsing_data/conditional_cache_deletion_helper.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace content {

StoragePartitionHttpCacheDataRemover::StoragePartitionHttpCacheDataRemover(
    base::Callback<bool(const GURL&)> url_predicate,
    base::Time delete_begin,
    base::Time delete_end,
    net::URLRequestContextGetter* main_context_getter,
    net::URLRequestContextGetter* media_context_getter,
    GeneratedCodeCacheContext* generated_code_cache_context)
    : url_predicate_(url_predicate),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      main_context_getter_(main_context_getter),
      media_context_getter_(media_context_getter),
      generated_code_cache_context_(generated_code_cache_context),
      next_cache_state_(CacheState::NONE),
      cache_(nullptr) {}

// static.
StoragePartitionHttpCacheDataRemover*
StoragePartitionHttpCacheDataRemover::CreateForRange(
    content::StoragePartition* storage_partition,
    base::Time delete_begin,
    base::Time delete_end) {
  return new StoragePartitionHttpCacheDataRemover(
      base::Callback<bool(const GURL&)>(),  // Null callback.
      delete_begin, delete_end, storage_partition->GetURLRequestContext(),
      storage_partition->GetMediaURLRequestContext(),
      storage_partition->GetGeneratedCodeCacheContext());
}

// static.
StoragePartitionHttpCacheDataRemover*
StoragePartitionHttpCacheDataRemover::CreateForURLsAndRange(
    content::StoragePartition* storage_partition,
    const base::Callback<bool(const GURL&)>& url_predicate,
    base::Time delete_begin,
    base::Time delete_end) {
  return new StoragePartitionHttpCacheDataRemover(
      url_predicate, delete_begin, delete_end,
      storage_partition->GetURLRequestContext(),
      storage_partition->GetMediaURLRequestContext(),
      storage_partition->GetGeneratedCodeCacheContext());
}

StoragePartitionHttpCacheDataRemover::~StoragePartitionHttpCacheDataRemover() {}

void StoragePartitionHttpCacheDataRemover::Remove(
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!done_callback.is_null());
  done_callback_ = std::move(done_callback);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &StoragePartitionHttpCacheDataRemover::ClearHttpCacheOnIOThread,
          base::Unretained(this)));
}

void StoragePartitionHttpCacheDataRemover::ClearHttpCacheOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  next_cache_state_ = CacheState::NONE;
  DCHECK_EQ(CacheState::NONE, next_cache_state_);

  next_cache_state_ = CacheState::CREATE_MAIN;
  DoClearCache(net::OK);
}

void StoragePartitionHttpCacheDataRemover::ClearedHttpCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(done_callback_).Run();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

// The expected state sequence is CacheState::NONE --> CacheState::CREATE_MAIN
// --> CacheState::DELETE_MAIN --> CacheState::CREATE_MEDIA -->
// CacheState::DELETE_MEDIA --> CacheState::DELETE_CODE --> CacheState::DONE,
// and any errors are ignored.
void StoragePartitionHttpCacheDataRemover::DoClearCache(int rv) {
  DCHECK_NE(CacheState::NONE, next_cache_state_);

  while (rv != net::ERR_IO_PENDING && next_cache_state_ != CacheState::NONE) {
    switch (next_cache_state_) {
      case CacheState::CREATE_MAIN:
      case CacheState::CREATE_MEDIA: {
        // Get a pointer to the cache.
        net::URLRequestContextGetter* getter =
            (next_cache_state_ == CacheState::CREATE_MAIN)
                ? main_context_getter_.get()
                : media_context_getter_.get();

        // Caches might not exist in tests.
        if (!getter) {
          next_cache_state_ = (next_cache_state_ == CacheState::CREATE_MAIN)
                                  ? CacheState::CREATE_MEDIA
                                  : CacheState::DELETE_CODE;
          break;
        }

        net::HttpCache* http_cache = getter->GetURLRequestContext()
                                         ->http_transaction_factory()
                                         ->GetCache();

        next_cache_state_ = (next_cache_state_ == CacheState::CREATE_MAIN)
                                ? CacheState::DELETE_MAIN
                                : CacheState::DELETE_MEDIA;

        // Clear QUIC server information from memory and the disk cache.
        http_cache->GetSession()
            ->quic_stream_factory()
            ->ClearCachedStatesInCryptoConfig(url_predicate_);

        rv = http_cache->GetBackend(
            &cache_,
            base::BindOnce(&StoragePartitionHttpCacheDataRemover::DoClearCache,
                           base::Unretained(this)));
        break;
      }
      case CacheState::DELETE_MAIN:
      case CacheState::DELETE_MEDIA: {
        next_cache_state_ = (next_cache_state_ == CacheState::DELETE_MAIN)
                                ? CacheState::CREATE_MEDIA
                                : CacheState::DELETE_CODE;

        // |cache_| can be null if it cannot be initialized.
        if (cache_) {
          if (!url_predicate_.is_null()) {
            rv = (new ConditionalCacheDeletionHelper(
                      cache_,
                      ConditionalCacheDeletionHelper::CreateURLAndTimeCondition(
                          url_predicate_, delete_begin_, delete_end_)))
                     ->DeleteAndDestroySelfWhenFinished(base::Bind(
                         &StoragePartitionHttpCacheDataRemover::DoClearCache,
                         base::Unretained(this)));
          } else if (delete_begin_.is_null() && delete_end_.is_max()) {
            rv = cache_->DoomAllEntries(base::BindOnce(
                &StoragePartitionHttpCacheDataRemover::DoClearCache,
                base::Unretained(this)));
          } else {
            rv = cache_->DoomEntriesBetween(
                delete_begin_, delete_end_,
                base::BindOnce(
                    &StoragePartitionHttpCacheDataRemover::DoClearCache,
                    base::Unretained(this)));
          }
          cache_ = nullptr;
        }
        break;
      }
      case CacheState::DELETE_CODE: {
        next_cache_state_ = CacheState::DONE;
        if (base::FeatureList::IsEnabled(features::kIsolatedCodeCache)) {
          DCHECK(generated_code_cache_context_);
          GeneratedCodeCache* code_cache =
              generated_code_cache_context_->generated_code_cache();
          if (code_cache) {
            auto callback = base::BindRepeating(
                &StoragePartitionHttpCacheDataRemover::DoClearCache,
                base::Unretained(this));
            // TODO(crbug.com/866419): Currently we just clear the entire cache.
            // Change it to conditionally clear the entries based on the
            // filters.
            rv = code_cache->ClearCache(callback);
          }
        }
        break;
      }
      case CacheState::DONE: {
        cache_ = nullptr;
        next_cache_state_ = CacheState::NONE;

        // Notify the UI thread that we are done.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(
                &StoragePartitionHttpCacheDataRemover::ClearedHttpCache,
                base::Unretained(this)));
        return;
      }
      case CacheState::NONE: {
        NOTREACHED() << "bad state";
        return;
      }
    }
  }
}

}  // namespace content
