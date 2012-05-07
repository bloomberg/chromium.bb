// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_quota_client.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
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
      base::MessageLoopProxy* db_tracker_thread)
      : QuotaThreadTask(client, db_tracker_thread),
        client_(client), db_tracker_(client->db_tracker_) {
  }

  virtual ~HelperTask() {}

  DatabaseQuotaClient* client_;
  scoped_refptr<DatabaseTracker> db_tracker_;
};

class DatabaseQuotaClient::GetOriginUsageTask : public HelperTask {
 public:
  GetOriginUsageTask(
      DatabaseQuotaClient* client,
      base::MessageLoopProxy* db_tracker_thread,
      const GURL& origin_url)
      : HelperTask(client, db_tracker_thread),
        origin_url_(origin_url), usage_(0) {
  }

 protected:
  virtual ~GetOriginUsageTask() {}

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

 private:
  GURL origin_url_;
  int64 usage_;
};

class DatabaseQuotaClient::GetOriginsTaskBase : public HelperTask {
 protected:
  GetOriginsTaskBase(
      DatabaseQuotaClient* client,
      base::MessageLoopProxy* db_tracker_thread)
      : HelperTask(client, db_tracker_thread) {
  }

  virtual ~GetOriginsTaskBase() {}

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
      base::MessageLoopProxy* db_tracker_thread,
      quota::StorageType type)
      : GetOriginsTaskBase(client, db_tracker_thread),
        type_(type) {
  }

 protected:
  virtual ~GetAllOriginsTask() {}

  virtual bool ShouldAddOrigin(const GURL& origin) OVERRIDE {
    return true;
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetAllOrigins(origins_, type_);
  }

 private:
  quota::StorageType type_;
};

class DatabaseQuotaClient::GetOriginsForHostTask : public GetOriginsTaskBase {
 public:
  GetOriginsForHostTask(
      DatabaseQuotaClient* client,
      base::MessageLoopProxy* db_tracker_thread,
      const std::string& host,
      quota::StorageType type)
      : GetOriginsTaskBase(client, db_tracker_thread),
        host_(host),
        type_(type) {
  }

 protected:
  virtual ~GetOriginsForHostTask() {}

  virtual bool ShouldAddOrigin(const GURL& origin) OVERRIDE {
    return host_ == net::GetHostOrSpecFromURL(origin);
  }

  virtual void Completed() OVERRIDE {
    client_->DidGetOriginsForHost(host_, origins_, type_);
  }

 private:
  std::string host_;
  quota::StorageType type_;
};

class DatabaseQuotaClient::DeleteOriginTask : public HelperTask {
 public:
  DeleteOriginTask(
      DatabaseQuotaClient* client,
      base::MessageLoopProxy* db_tracker_thread,
      const GURL& origin_url,
      const DeletionCallback& caller_callback)
      : HelperTask(client, db_tracker_thread),
        origin_url_(origin_url),
        result_(quota::kQuotaStatusUnknown),
        caller_callback_(caller_callback) {
  }

 protected:
  virtual ~DeleteOriginTask() {}

  virtual void Completed() OVERRIDE {
    if (caller_callback_.is_null())
      return;
    caller_callback_.Run(result_);
    caller_callback_.Reset();
  }

  virtual void Aborted() OVERRIDE {
    caller_callback_.Reset();
  }

  virtual bool RunOnTargetThreadAsync() OVERRIDE {
    AddRef();  // balanced in OnCompletionCallback
    string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url_);
    int rv = db_tracker_->DeleteDataForOrigin(
        origin_id, base::Bind(&DeleteOriginTask::OnCompletionCallback,
                              base::Unretained(this)));
    if (rv == net::ERR_IO_PENDING)
      return false;  // we wait for the callback
    OnCompletionCallback(rv);
    return false;
  }

 private:
  void OnCompletionCallback(int rv) {
    if (rv == net::OK)
      result_ = quota::kQuotaStatusOk;
    original_task_runner()->PostTask(
        FROM_HERE, base::Bind(&DeleteOriginTask::CallCompleted, this));
    Release();  // balanced in RunOnTargetThreadAsync
  }

  const GURL origin_url_;
  quota::QuotaStatusCode result_;
  DeletionCallback caller_callback_;
  net::CompletionCallback completion_callback_;
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
    const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(0);
    return;
  }

  if (usage_for_origin_callbacks_.Add(origin_url, callback)) {
    scoped_refptr<GetOriginUsageTask> task(
        new GetOriginUsageTask(this, db_tracker_thread_, origin_url));
    task->Start();
  }
}

void DatabaseQuotaClient::GetOriginsForType(
    quota::StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>(), type);
    return;
  }

  if (origins_for_type_callbacks_.Add(callback)) {
    scoped_refptr<GetAllOriginsTask> task(
        new GetAllOriginsTask(this, db_tracker_thread_, type));
    task->Start();
  }
}

void DatabaseQuotaClient::GetOriginsForHost(
    quota::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>(), type);
    return;
  }

  if (origins_for_host_callbacks_.Add(host, callback)) {
    scoped_refptr<GetOriginsForHostTask> task(
        new GetOriginsForHostTask(this, db_tracker_thread_, host, type));
    task->Start();
  }
}

void DatabaseQuotaClient::DeleteOriginData(const GURL& origin,
                                           quota::StorageType type,
                                           const DeletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now, so nothing to delete.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(quota::kQuotaStatusOk);
    return;
  }

  scoped_refptr<DeleteOriginTask> task(
      new DeleteOriginTask(this, db_tracker_thread_,
                           origin, callback));
  task->Start();
}

void DatabaseQuotaClient::DidGetOriginUsage(
    const GURL& origin_url, int64 usage) {
  DCHECK(usage_for_origin_callbacks_.HasCallbacks(origin_url));
  usage_for_origin_callbacks_.Run(origin_url, usage);
}

void DatabaseQuotaClient::DidGetAllOrigins(const std::set<GURL>& origins,
    quota::StorageType type) {
  DCHECK(origins_for_type_callbacks_.HasCallbacks());
  origins_for_type_callbacks_.Run(origins, type);
}

void DatabaseQuotaClient::DidGetOriginsForHost(
    const std::string& host, const std::set<GURL>& origins,
    quota::StorageType type) {
  DCHECK(origins_for_host_callbacks_.HasCallbacks(host));
  origins_for_host_callbacks_.Run(host, origins, type);
}

}  // namespace webkit_database
