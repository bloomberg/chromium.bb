// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_context_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

namespace content {

CacheStorageContextImpl::CacheStorageContextImpl(
    BrowserContext* browser_context)
    : task_runner_(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})),
      observers_(base::MakeRefCounted<ObserverList>()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CacheStorageContextImpl::~CacheStorageContextImpl() {
  // Can be destroyed on any thread.
}

void CacheStorageContextImpl::Init(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_incognito_ = user_data_directory.empty();
  special_storage_policy_ = std::move(special_storage_policy);

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::CreateCacheStorageManager, this,
                     user_data_directory, std::move(cache_task_runner),
                     std::move(quota_manager_proxy)));
}

void CacheStorageContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::ShutdownOnTaskRunner, this));
}

void CacheStorageContextImpl::AddBinding(
    blink::mojom::CacheStorageRequest request,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dispatcher_host_) {
    dispatcher_host_ =
        base::SequenceBound<CacheStorageDispatcherHost>(task_runner_);
    dispatcher_host_.Post(FROM_HERE, &CacheStorageDispatcherHost::Init,
                          base::RetainedRef(this));
  }
  dispatcher_host_.Post(FROM_HERE, &CacheStorageDispatcherHost::AddBinding,
                        std::move(request), origin);
}

CacheStorageManager* CacheStorageContextImpl::cache_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return cache_manager_.get();
}

void CacheStorageContextImpl::SetBlobParametersForCache(
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CacheStorageContextImpl::SetBlobParametersForCacheOnTaskRunner, this,
          base::RetainedRef(blob_storage_context)));
}

void CacheStorageContextImpl::GetAllOriginsInfo(
    CacheStorageContext::GetUsageInfoCallback callback) {
  // Can be called on any sequence.

  callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
         GetUsageInfoCallback inner,
         const std::vector<StorageUsageInfo>& entries) {
        reply_task_runner->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(inner), entries));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<CacheStorageContextImpl> context,
             GetUsageInfoCallback callback) {
            if (!context->cache_manager()) {
              std::move(callback).Run(std::vector<StorageUsageInfo>());
              return;
            }
            context->cache_manager()->GetAllOriginsUsage(
                CacheStorageOwner::kCacheAPI, std::move(callback));
          },
          base::RetainedRef(this), std::move(callback)));
}

void CacheStorageContextImpl::DeleteForOrigin(const GURL& origin) {
  // Can be called on any sequence.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(
                             [](scoped_refptr<CacheStorageContextImpl> context,
                                const GURL& origin) {
                               if (!context->cache_manager())
                                 return;
                               context->cache_manager()->DeleteOriginData(
                                   url::Origin::Create(origin),
                                   CacheStorageOwner::kCacheAPI);
                             },
                             base::RetainedRef(this), origin));
}

void CacheStorageContextImpl::AddObserver(
    CacheStorageContextImpl::Observer* observer) {
  // Any sequence
  observers_->AddObserver(observer);
}

void CacheStorageContextImpl::RemoveObserver(
    CacheStorageContextImpl::Observer* observer) {
  // Any sequence
  observers_->RemoveObserver(observer);
}

void CacheStorageContextImpl::CreateCacheStorageManager(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!cache_manager_);
  cache_manager_ = LegacyCacheStorageManager::Create(
      user_data_directory, std::move(cache_task_runner), task_runner_,
      std::move(quota_manager_proxy), observers_);
}

void CacheStorageContextImpl::ShutdownOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Delete session-only ("clear on exit") origins.
  if (special_storage_policy_ &&
      special_storage_policy_->HasSessionOnlyOrigins()) {
    cache_manager_->GetAllOriginsUsage(
        CacheStorageOwner::kCacheAPI,
        // TODO(jsbell): Make this BindOnce.
        base::BindRepeating(
            [](scoped_refptr<CacheStorageManager> cache_manager,
               scoped_refptr<storage::SpecialStoragePolicy>
                   special_storage_policy,
               const std::vector<StorageUsageInfo>& usage_info) {
              for (const auto& info : usage_info) {
                if (special_storage_policy->IsStorageSessionOnly(
                        info.origin.GetURL()) &&
                    !special_storage_policy->IsStorageProtected(
                        info.origin.GetURL())) {
                  cache_manager->DeleteOriginData(
                      info.origin, CacheStorageOwner::kCacheAPI,

                      // Retain a reference to the manager until the deletion is
                      // complete, since it internally uses weak pointers for
                      // the various stages of deletion and nothing else will
                      // keep it alive during shutdown.
                      base::BindOnce(
                          [](scoped_refptr<CacheStorageManager> cache_manager,
                             blink::mojom::QuotaStatusCode) {},
                          cache_manager));
                }
              }
            },
            cache_manager_, special_storage_policy_));
  }

  // Release |cache_manager_|. New clients will get a nullptr if they request
  // an instance of CacheStorageManager after this. Any other client that
  // ref-wrapped |cache_manager_| will be able to continue using it, and the
  // CacheStorageManager will be destroyed when all the references are
  // destroyed.
  cache_manager_ = nullptr;
}

void CacheStorageContextImpl::SetBlobParametersForCacheOnTaskRunner(
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (cache_manager_ && blob_storage_context) {
    cache_manager_->SetBlobParametersForCache(
        blob_storage_context->context()->AsWeakPtr());
  }
}

}  // namespace content
