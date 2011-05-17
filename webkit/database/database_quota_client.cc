// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_quota_client.h"

#include <vector>

#include "base/message_loop_proxy.h"
#include "net/base/net_util.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"

using quota::QuotaClient;

namespace webkit_database {

// Helper tasks ---------------------------------------------------------------

class DatabaseQuotaClient::HelperTask : public quota::QuotaThreadTask {
 protected:
  HelperTask(
      DatabaseQuotaClient* client,
      scoped_refptr<base::MessageLoopProxy> db_tracker_thread)
      : QuotaThreadTask(client, db_tracker_thread),
        client_(client), db_tracker_(client->db_tracker_) {
  }

  DatabaseQuotaClient* client_;
  scoped_refptr<DatabaseTracker> db_tracker_;
};

class DatabaseQuotaClient::GetOriginUsageTask : public HelperTask {
 public:
  GetOriginUsageTask(
      DatabaseQuotaClient* client,
      scoped_refptr<base::MessageLoopProxy> db_tracker_thread,
      const GURL& origin_url)
      : HelperTask(client, db_tracker_thread),
        origin_url_(origin_url), usage_(0) {
  }

 private:
  virtual void RunOnTargetThread() OVERRIDE {
    OriginInfo info;
    if (db_tracker_->GetOriginInfo(
            DatabaseUtil::GetOriginIdentifier(origin_url_),
            &info)) {
      usage_ = info.TotalSize();
    }
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetOriginUsage(origin_url_, usage_);
  }
  GURL origin_url_;
  int64 usage_;
};

class DatabaseQuotaClient::GetOriginsTaskBase : public HelperTask {
 protected:
  GetOriginsTaskBase(
      DatabaseQuotaClient* client,
      scoped_refptr<base::MessageLoopProxy> db_tracker_thread)
      : HelperTask(client, db_tracker_thread) {
  }

  virtual bool ShouldAddOrigin(const GURL& origin) = 0;

  virtual void RunOnTargetThread() OVERRIDE {
    std::vector<string16> origin_identifiers;
    if (db_tracker_->GetAllOriginIdentifiers(&origin_identifiers)) {
      for (std::vector<string16>::const_iterator iter =
               origin_identifiers.begin();
           iter != origin_identifiers.end(); ++iter) {
        GURL origin = DatabaseUtil::GetOriginFromIdentifier(*iter);
        if (ShouldAddOrigin(origin))
          origins_.insert(origin);
      }
    }
  }

  std::set<GURL> origins_;
};

class DatabaseQuotaClient::GetAllOriginsTask : public GetOriginsTaskBase {
 public:
  GetAllOriginsTask(
      DatabaseQuotaClient* client,
      scoped_refptr<base::MessageLoopProxy> db_tracker_thread)
      : GetOriginsTaskBase(client, db_tracker_thread) {
  }

 protected:
  virtual bool ShouldAddOrigin(const GURL& origin) OVERRIDE {
    return true;
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetAllOrigins(origins_);
  }
};

class DatabaseQuotaClient::GetOriginsForHostTask : public GetOriginsTaskBase {
 public:
  GetOriginsForHostTask(
      DatabaseQuotaClient* client,
      scoped_refptr<base::MessageLoopProxy> db_tracker_thread,
      const std::string& host)
      : GetOriginsTaskBase(client, db_tracker_thread),
        host_(host) {
  }

 private:
  virtual bool ShouldAddOrigin(const GURL& origin) OVERRIDE {
    return host_ == net::GetHostOrSpecFromURL(origin);
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetOriginsForHost(host_, origins_);
  }
  std::string host_;
};

// DatabaseQuotaClient --------------------------------------------------------

DatabaseQuotaClient::DatabaseQuotaClient(
    base::MessageLoopProxy* db_tracker_thread,
    DatabaseTracker* db_tracker)
    : db_tracker_thread_(db_tracker_thread), db_tracker_(db_tracker) {
}

DatabaseQuotaClient::~DatabaseQuotaClient() {
}

QuotaClient::ID DatabaseQuotaClient::id() const {
  return kDatabase;
}

void DatabaseQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void DatabaseQuotaClient::GetOriginUsage(
    const GURL& origin_url,
    quota::StorageType type,
    GetUsageCallback* callback_ptr) {
  DCHECK(callback_ptr);
  DCHECK(db_tracker_.get());
  scoped_ptr<GetUsageCallback> callback(callback_ptr);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback->Run(0);
    return;
  }

  if (usage_for_origin_callbacks_.Add(origin_url, callback.release())) {
    scoped_refptr<GetOriginUsageTask> task(
        new GetOriginUsageTask(this, db_tracker_thread_, origin_url));
    task->Start();
  }
}

void DatabaseQuotaClient::GetOriginsForType(
    quota::StorageType type,
    GetOriginsCallback* callback_ptr) {
  DCHECK(callback_ptr);
  DCHECK(db_tracker_.get());
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback->Run(std::set<GURL>());
    return;
  }

  if (origins_for_type_callbacks_.Add(callback.release())) {
    scoped_refptr<GetAllOriginsTask> task(
        new GetAllOriginsTask(this, db_tracker_thread_));
    task->Start();
  }
}

void DatabaseQuotaClient::GetOriginsForHost(
    quota::StorageType type,
    const std::string& host,
    GetOriginsCallback* callback_ptr) {
  DCHECK(callback_ptr);
  DCHECK(db_tracker_.get());
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback->Run(std::set<GURL>());
    return;
  }

  if (origins_for_host_callbacks_.Add(host, callback.release())) {
    scoped_refptr<GetOriginsForHostTask> task(
        new GetOriginsForHostTask(this, db_tracker_thread_, host));
    task->Start();
  }
}

void DatabaseQuotaClient::DeleteOriginData(const GURL& origin,
                                           quota::StorageType type,
                                           DeletionCallback* callback) {
  // TODO(tzik): implement me
  callback->Run(quota::kQuotaErrorNotSupported);
  delete callback;
}

void DatabaseQuotaClient::DidGetOriginUsage(
    const GURL& origin_url, int64 usage) {
  DCHECK(usage_for_origin_callbacks_.HasCallbacks(origin_url));
  usage_for_origin_callbacks_.Run(origin_url, usage);
}

void DatabaseQuotaClient::DidGetAllOrigins(const std::set<GURL>& origins) {
  DCHECK(origins_for_type_callbacks_.HasCallbacks());
  origins_for_type_callbacks_.Run(origins);
}

void DatabaseQuotaClient::DidGetOriginsForHost(
    const std::string& host, const std::set<GURL>& origins) {
  DCHECK(origins_for_host_callbacks_.HasCallbacks(host));
  origins_for_host_callbacks_.Run(host, origins);
}

}  // namespace webkit_database
