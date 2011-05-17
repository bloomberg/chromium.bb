// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/mock_storage_client.h"

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "net/base/net_util.h"
#include "webkit/quota/quota_manager.h"

using base::AtomicSequenceNumber;

namespace quota {

namespace {

using std::make_pair;

class MockStorageClientIDSequencer {
 public:
  static MockStorageClientIDSequencer* GetInstance() {
    return Singleton<MockStorageClientIDSequencer>::get();
  }

  QuotaClient::ID NextMockID() {
    return static_cast<QuotaClient::ID>(
        QuotaClient::kMockStart + seq_.GetNext());
  }

 private:
  MockStorageClientIDSequencer() { }
  friend struct DefaultSingletonTraits<MockStorageClientIDSequencer>;
  AtomicSequenceNumber seq_;

  DISALLOW_COPY_AND_ASSIGN(MockStorageClientIDSequencer);
};

}  // anonymous namespace

MockStorageClient::MockStorageClient(QuotaManagerProxy* quota_manager_proxy)
    : quota_manager_proxy_(quota_manager_proxy),
      id_(MockStorageClientIDSequencer::GetInstance()->NextMockID()),
      runnable_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

MockStorageClient::~MockStorageClient() {
  STLDeleteContainerPointers(usage_callbacks_.begin(), usage_callbacks_.end());
  STLDeleteContainerPointers(
      origins_callbacks_.begin(), origins_callbacks_.end());
  STLDeleteContainerPointers(
      deletion_callbacks_.begin(), deletion_callbacks_.end());
}

void MockStorageClient::AddMockOriginData(
    const GURL& origin_url, StorageType type, int64 size) {
  origin_data_[make_pair(origin_url, type)] = size;
}

void MockStorageClient::ModifyMockOriginDataSize(
    const GURL& origin_url, StorageType type, int64 delta) {
  OriginDataMap::iterator find = origin_data_.find(make_pair(origin_url, type));
  if (find == origin_data_.end()) {
    DCHECK_GE(delta, 0);
    AddMockOriginData(origin_url, type, delta);
    return;
  }
  find->second += delta;
  DCHECK_GE(find->second, 0);

  // TODO(tzik): Check quota to prevent usage exceed
  quota_manager_proxy_->NotifyStorageModified(id(), origin_url, type, delta);
}

QuotaClient::ID MockStorageClient::id() const {
  return id_;
}

void MockStorageClient::OnQuotaManagerDestroyed() {
  delete this;
}

void MockStorageClient::GetOriginUsage(const GURL& origin_url,
                                       StorageType type,
                                       GetUsageCallback* callback) {
  usage_callbacks_.insert(callback);
  base::MessageLoopProxy::CreateForCurrentThread()->PostTask(
      FROM_HERE, runnable_factory_.NewRunnableMethod(
          &MockStorageClient::RunGetOriginUsage,
          origin_url, type, callback));
}

void MockStorageClient::GetOriginsForType(
    StorageType type, GetOriginsCallback* callback) {
  origins_callbacks_.insert(callback);
  base::MessageLoopProxy::CreateForCurrentThread()->PostTask(
      FROM_HERE, runnable_factory_.NewRunnableMethod(
          &MockStorageClient::RunGetOriginsForType,
          type, callback));
}

void MockStorageClient::GetOriginsForHost(
    StorageType type, const std::string& host,
    GetOriginsCallback* callback) {
  origins_callbacks_.insert(callback);
  base::MessageLoopProxy::CreateForCurrentThread()->PostTask(
      FROM_HERE, runnable_factory_.NewRunnableMethod(
          &MockStorageClient::RunGetOriginsForHost,
          type, host, callback));
}

void MockStorageClient::DeleteOriginData(
    const GURL& origin, StorageType type,
    DeletionCallback* callback) {
  deletion_callbacks_.insert(callback);
  base::MessageLoopProxy::CreateForCurrentThread()->PostTask(
      FROM_HERE, runnable_factory_.NewRunnableMethod(
          &MockStorageClient::RunDeleteOriginData,
          origin, type, callback));
}

void MockStorageClient::RunGetOriginUsage(
    const GURL& origin_url, StorageType type, GetUsageCallback* callback_ptr) {
  usage_callbacks_.erase(callback_ptr);
  scoped_ptr<GetUsageCallback> callback(callback_ptr);
  OriginDataMap::iterator find = origin_data_.find(make_pair(origin_url, type));
  if (find == origin_data_.end()) {
    callback->Run(0);
  } else {
    callback->Run(find->second);
  }
}

void MockStorageClient::RunGetOriginsForType(
    StorageType type, GetOriginsCallback* callback_ptr) {
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);
  origins_callbacks_.erase(callback_ptr);
  std::set<GURL> origins;
  for (OriginDataMap::iterator iter = origin_data_.begin();
       iter != origin_data_.end(); ++iter) {
    if (type == iter->first.second)
      origins.insert(iter->first.first);
  }
  callback->Run(origins);
}

void MockStorageClient::RunGetOriginsForHost(
    StorageType type, const std::string& host,
    GetOriginsCallback* callback_ptr) {
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);
  origins_callbacks_.erase(callback_ptr);
  std::set<GURL> origins;
  for (OriginDataMap::iterator iter = origin_data_.begin();
       iter != origin_data_.end(); ++iter) {
    std::string host_or_spec = net::GetHostOrSpecFromURL(iter->first.first);
    if (type == iter->first.second && host == host_or_spec)
      origins.insert(iter->first.first);
  }
  callback->Run(origins);
}

void MockStorageClient::RunDeleteOriginData(
    const GURL& origin_url,
    StorageType type,
    DeletionCallback* callback_ptr) {
  OriginDataMap::iterator itr
      = origin_data_.find(make_pair(origin_url, type));
  if (itr != origin_data_.end()) {
    int64 delta = itr->second;
    quota_manager_proxy_->
        NotifyStorageModified(id(), origin_url, type, -delta);
    origin_data_.erase(itr);
  }

  scoped_ptr<DeletionCallback> callback(callback_ptr);
  deletion_callbacks_.erase(callback_ptr);
  callback->Run(kQuotaStatusOk);
}

}  // namespace quota
