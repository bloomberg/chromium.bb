// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/mock_quota_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

namespace quota {

class MockQuotaManager::GetModifiedSinceTask : public QuotaThreadTask {
 public:
  GetModifiedSinceTask(MockQuotaManager* manager,
                       const std::set<GURL>& origins,
                       StorageType type,
                       GetOriginsCallback* callback)
      : QuotaThreadTask(manager, manager->io_thread_),
        origins_(origins),
        type_(type),
        callback_(callback) {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {}

  virtual void Completed() OVERRIDE {
    callback_->Run(origins_, type_);
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(std::set<GURL>(), type_);
  }

 private:
  std::set<GURL> origins_;
  StorageType type_;
  scoped_ptr<GetOriginsCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(GetModifiedSinceTask);
};

class MockQuotaManager::DeleteOriginDataTask : public QuotaThreadTask {
 public:
  DeleteOriginDataTask(MockQuotaManager* manager,
                       StatusCallback* callback)
      : QuotaThreadTask(manager, manager->io_thread_),
        callback_(callback) {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {}

  virtual void Completed() OVERRIDE {
    callback_->Run(quota::kQuotaStatusOk);
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(quota::kQuotaErrorAbort);
  }

 private:
  scoped_ptr<StatusCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOriginDataTask);
};

MockQuotaManager::OriginInfo::OriginInfo(
    const GURL& origin,
    StorageType type,
    base::Time modified)
    : origin(origin),
      type(type),
      modified(modified) {
}

MockQuotaManager::OriginInfo::~OriginInfo() {}

MockQuotaManager::MockQuotaManager(bool is_incognito,
    const FilePath& profile_path, base::MessageLoopProxy* io_thread,
    base::MessageLoopProxy* db_thread,
    SpecialStoragePolicy* special_storage_policy)
    : QuotaManager(is_incognito, profile_path, io_thread, db_thread,
        special_storage_policy) {
}

MockQuotaManager::~MockQuotaManager() {}

bool MockQuotaManager::AddOrigin(const GURL& origin, StorageType type,
    base::Time modified) {
  origins_.push_back(OriginInfo(origin, type, modified));
  return true;
}

bool MockQuotaManager::OriginHasData(const GURL& origin,
    StorageType type) const {
  for (std::vector<OriginInfo>::const_iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->origin == origin && current->type == type)
      return true;
  }
  return false;
}

void MockQuotaManager::GetOriginsModifiedSince(StorageType type,
    base::Time modified_since, GetOriginsCallback* callback) {
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

void MockQuotaManager::DeleteOriginData(const GURL& origin, StorageType type,
    StatusCallback* callback) {
  for (std::vector<OriginInfo>::iterator current = origins_.begin();
       current != origins_.end();
       ++current) {
    if (current->origin == origin && current->type == type) {
      origins_.erase(current);
      break;
    }
  }
  make_scoped_refptr(new DeleteOriginDataTask(this, callback))->Start();
}

}  // namespace quota
