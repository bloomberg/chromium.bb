// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using content::BrowserThread;
using content::BrowserContext;

// static
scoped_refptr<BrowsingDataQuotaHelper> BrowsingDataQuotaHelper::Create(
    Profile* profile) {
  return base::MakeRefCounted<BrowsingDataQuotaHelperImpl>(
      profile->GetDefaultStoragePartition()->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread,
                     this, std::move(callback)));
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuota(const std::string& host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataQuotaHelperImpl::RevokeHostQuotaOnIOThread,
                     this, host));
}

BrowsingDataQuotaHelperImpl::BrowsingDataQuotaHelperImpl(
    storage::QuotaManager* quota_manager)
    : BrowsingDataQuotaHelper(), quota_manager_(quota_manager) {
  DCHECK(quota_manager);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() {}

void BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread(
    FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const StorageType types[] = {StorageType::kTemporary,
                               StorageType::kPersistent,
                               StorageType::kSyncable};

  // Query hosts for each storage types. When complete, process the collected
  // hosts.
  PendingHosts* pending_hosts = new PendingHosts();
  base::RepeatingClosure completion = base::BarrierClosure(
      base::size(types),
      base::BindOnce(&BrowsingDataQuotaHelperImpl::OnGetOriginsComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(pending_hosts)));

  for (const StorageType& type : types) {
    quota_manager_->GetStorageKeysForType(
        type, base::BindOnce(&BrowsingDataQuotaHelperImpl::GotStorageKeys,
                             weak_factory_.GetWeakPtr(), pending_hosts,
                             completion, type));
  }
}

void BrowsingDataQuotaHelperImpl::GotStorageKeys(
    PendingHosts* pending_hosts,
    base::OnceClosure completion,
    StorageType type,
    const std::set<blink::StorageKey>& storage_keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const blink::StorageKey& storage_key : storage_keys) {
    if (!browsing_data::IsWebScheme(storage_key.origin().scheme()))
      continue;  // Non-websafe state is not considered browsing data.
    pending_hosts->insert(std::make_pair(storage_key.origin().host(), type));
  }
  std::move(completion).Run();
}

void BrowsingDataQuotaHelperImpl::OnGetOriginsComplete(
    FetchResultCallback callback,
    PendingHosts* pending_hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Query usage for each (host, type). When complete, process the results.
  QuotaInfoMap* quota_info = new QuotaInfoMap();
  base::RepeatingClosure completion = base::BarrierClosure(
      pending_hosts->size(),
      base::BindOnce(&BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(quota_info)));

  for (const auto& itr : *pending_hosts) {
    const std::string& host = itr.first;
    StorageType type = itr.second;
    quota_manager_->GetHostUsageWithBreakdown(
        host, type,
        base::BindOnce(&BrowsingDataQuotaHelperImpl::GotHostUsage,
                       weak_factory_.GetWeakPtr(), quota_info, completion, host,
                       type));
  }
}

void BrowsingDataQuotaHelperImpl::GotHostUsage(
    QuotaInfoMap* quota_info,
    base::OnceClosure completion,
    const std::string& host,
    StorageType type,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (type) {
    case StorageType::kTemporary:
      (*quota_info)[host].temporary_usage = usage;
      break;
    case StorageType::kPersistent:
      (*quota_info)[host].persistent_usage = usage;
      break;
    case StorageType::kSyncable:
      (*quota_info)[host].syncable_usage = usage;
      break;
    default:
      NOTREACHED();
  }
  std::move(completion).Run();
}

void BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete(
    FetchResultCallback callback,
    QuotaInfoMap* quota_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  QuotaInfoArray result;
  for (auto& pair : *quota_info) {
    QuotaInfo& info = pair.second;
    // Skip unused entries
    if (info.temporary_usage <= 0 && info.persistent_usage <= 0 &&
        info.syncable_usage <= 0)
      continue;

    info.host = pair.first;
    result.push_back(info);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuotaOnIOThread(
    const std::string& host) {
  quota_manager_->SetPersistentHostQuota(
      host, 0,
      base::BindOnce(&BrowsingDataQuotaHelperImpl::DidRevokeHostQuota,
                     weak_factory_.GetWeakPtr()));
}

void BrowsingDataQuotaHelperImpl::DidRevokeHostQuota(
    blink::mojom::QuotaStatusCode /*status*/,
    int64_t /*quota*/) {}
