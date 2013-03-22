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
#include "base/task_runner_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"

using quota::QuotaClient;

namespace webkit_database {

namespace {

int64 GetOriginUsageOnDBThread(
    DatabaseTracker* db_tracker,
    const GURL& origin_url) {
  OriginInfo info;
  if (db_tracker->GetOriginInfo(
          DatabaseUtil::GetOriginIdentifier(origin_url), &info))
    return info.TotalSize();
  return 0;
}

void GetOriginsOnDBThread(
    DatabaseTracker* db_tracker,
    std::set<GURL>* origins_ptr) {
  std::vector<string16> origin_identifiers;
  if (db_tracker->GetAllOriginIdentifiers(&origin_identifiers)) {
    for (std::vector<string16>::const_iterator iter =
         origin_identifiers.begin();
         iter != origin_identifiers.end(); ++iter) {
      GURL origin = DatabaseUtil::GetOriginFromIdentifier(*iter);
      origins_ptr->insert(origin);
    }
  }
}

void GetOriginsForHostOnDBThread(
    DatabaseTracker* db_tracker,
    std::set<GURL>* origins_ptr,
    const std::string& host) {
  std::vector<string16> origin_identifiers;
  if (db_tracker->GetAllOriginIdentifiers(&origin_identifiers)) {
    for (std::vector<string16>::const_iterator iter =
         origin_identifiers.begin();
         iter != origin_identifiers.end(); ++iter) {
      GURL origin = DatabaseUtil::GetOriginFromIdentifier(*iter);
      if (host == net::GetHostOrSpecFromURL(origin))
        origins_ptr->insert(origin);
    }
  }
}

void DidGetOrigins(
    const QuotaClient::GetOriginsCallback& callback,
    std::set<GURL>* origins_ptr,
    quota::StorageType type) {
  callback.Run(*origins_ptr, type);
}

void DidDeleteOriginData(
    base::SingleThreadTaskRunner* original_task_runner,
    const QuotaClient::DeletionCallback& callback,
    int result) {
  if (result == net::ERR_IO_PENDING) {
    // The callback will be invoked via
    // DatabaseTracker::ScheduleDatabasesForDeletion.
    return;
  }

  quota::QuotaStatusCode status;
  if (result == net::OK)
    status = quota::kQuotaStatusOk;
  else
    status = quota::kQuotaStatusUnknown;

  if (original_task_runner->BelongsToCurrentThread())
    callback.Run(status);
  else
    original_task_runner->PostTask(FROM_HERE, base::Bind(callback, status));
}

}  // namespace

DatabaseQuotaClient::DatabaseQuotaClient(
    base::MessageLoopProxy* db_tracker_thread,
    DatabaseTracker* db_tracker)
    : db_tracker_thread_(db_tracker_thread), db_tracker_(db_tracker) {
}

DatabaseQuotaClient::~DatabaseQuotaClient() {
  if (!db_tracker_thread_->RunsTasksOnCurrentThread()) {
    DatabaseTracker* tracker = db_tracker_;
    tracker->AddRef();
    db_tracker_ = NULL;
    db_tracker_thread_->ReleaseSoon(FROM_HERE, tracker);
  }
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

  base::PostTaskAndReplyWithResult(
      db_tracker_thread_,
      FROM_HERE,
      base::Bind(&GetOriginUsageOnDBThread,
                 db_tracker_,
                 origin_url),
      callback);
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

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  db_tracker_thread_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsOnDBThread,
                 db_tracker_,
                 base::Unretained(origins_ptr)),
      base::Bind(&DidGetOrigins,
                 callback,
                 base::Owned(origins_ptr),
                 type));
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

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  db_tracker_thread_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsForHostOnDBThread,
                 db_tracker_,
                 base::Unretained(origins_ptr),
                 host),
      base::Bind(&DidGetOrigins,
                 callback,
                 base::Owned(origins_ptr),
                 type));
}

void DatabaseQuotaClient::DeleteOriginData(
    const GURL& origin,
    quota::StorageType type,
    const DeletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now, so nothing to delete.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(quota::kQuotaStatusOk);
    return;
  }

  base::Callback<void(int)> delete_callback =
      base::Bind(&DidDeleteOriginData,
                 base::MessageLoopProxy::current(),
                 callback);

  PostTaskAndReplyWithResult(
      db_tracker_thread_,
      FROM_HERE,
      base::Bind(&DatabaseTracker::DeleteDataForOrigin,
                 db_tracker_,
                 DatabaseUtil::GetOriginIdentifier(origin),
                 delete_callback),
      delete_callback);
}

}  // namespace webkit_database
