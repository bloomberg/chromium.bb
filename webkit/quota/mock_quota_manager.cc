// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/mock_quota_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "googleurl/src/gurl.h"

namespace quota {

class MockQuotaManager::GetModifiedSinceTask : public QuotaThreadTask {
 public:
  GetModifiedSinceTask(MockQuotaManager* manager,
                       const std::set<GURL>& origins,
                       StorageType type,
                       const GetOriginsCallback& callback)
      : QuotaThreadTask(manager, manager->io_thread_.get()),
        origins_(origins),
        type_(type),
        callback_(callback) {
  }

 protected:
  virtual ~GetModifiedSinceTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {}

  virtual void Completed() OVERRIDE {
    callback_.Run(origins_, type_);
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(std::set<GURL>(), type_);
  }

 private:
  std::set<GURL> origins_;
  StorageType type_;
  GetOriginsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetModifiedSinceTask);
};

class MockQuotaManager::DeleteOriginDataTask : public QuotaThreadTask {
 public:
  DeleteOriginDataTask(MockQuotaManager* manager,
                       const StatusCallback& callback)
      : QuotaThreadTask(manager, manager->io_thread_),
        callback_(callback) {
  }

 protected:
  virtual ~DeleteOriginDataTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {}

  virtual void Completed() OVERRIDE {
    callback_.Run(quota::kQuotaStatusOk);
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(quota::kQuotaErrorAbort);
  }

 private:
  StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOriginDataTask);
};

MockQuotaManager::OriginInfo::OriginInfo(
    const GURL& origin,
    StorageType type,
    int quota_client_mask,
    base::Time modified)
    : origin(origin),
      type(type),
      quota_client_mask(quota_client_mask),
      modified(modified) {
}

MockQuotaManager::OriginInfo::~OriginInfo() {}

MockQuotaManager::MockQuotaManager(
    bool is_incognito,
    const FilePath& profile_path,
    base::SingleThreadTaskRunner* io_thread,
    base::SequencedTaskRunner* db_thread,
    SpecialStoragePolicy* special_storage_policy)
    : QuotaManager(is_incognito, profile_path, io_thread, db_thread,
        special_storage_policy) {
}

bool MockQuotaManager::AddOrigin(
    const GURL& origin,
    StorageType type,
    int quota_client_mask,
    base::Time modified) {
  origins_.push_back(OriginInfo(origin, type, quota_client_mask, modified));
  return true;
}

bool MockQuotaManager::OriginHasData(
    const GURL& origin,
    StorageType type,
    QuotaClient::ID quota_client) const {
  for (std::vector<OriginInfo>::const_iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->origin == origin &&
        current->type == type &&
        current->quota_client_mask & quota_client)
      return true;
  }
  return false;
}

void MockQuotaManager::GetOriginsModifiedSince(
    StorageType type,
    base::Time modified_since,
    const GetOriginsCallback& callback) {
  std::set<GURL> origins_to_return;
  for (std::vector<OriginInfo>::const_iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->type == type && current->modified >= modified_since)
      origins_to_return.insert(current->origin);
  }
  make_scoped_refptr(new GetModifiedSinceTask(this, origins_to_return, type,
      callback))->Start();
}

void MockQuotaManager::DeleteOriginData(
    const GURL& origin,
    StorageType type,
    int quota_client_mask,
    const StatusCallback& callback) {
  for (std::vector<OriginInfo>::iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->origin == origin && current->type == type) {
      // Modify the mask: if it's 0 after "deletion", remove the origin.
      current->quota_client_mask &= ~quota_client_mask;
      if (current->quota_client_mask == 0)
        origins_.erase(current);
      break;
    }
  }
  make_scoped_refptr(new DeleteOriginDataTask(this, callback))->Start();
}

MockQuotaManager::~MockQuotaManager() {}

}  // namespace quota
