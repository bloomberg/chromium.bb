// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_quota_client.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/database/database_util.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"

using blink::mojom::StorageType;
using storage::QuotaClient;

namespace storage {

namespace {

int64_t GetOriginUsageOnDBThread(DatabaseTracker* db_tracker,
                                 const GURL& origin_url) {
  OriginInfo info;
  if (db_tracker->GetOriginInfo(storage::GetIdentifierFromOrigin(origin_url),
                                &info))
    return info.TotalSize();
  return 0;
}

void GetOriginsOnDBThread(
    DatabaseTracker* db_tracker,
    std::set<GURL>* origins_ptr) {
  std::vector<std::string> origin_identifiers;
  if (db_tracker->GetAllOriginIdentifiers(&origin_identifiers)) {
    for (std::vector<std::string>::const_iterator iter =
         origin_identifiers.begin();
         iter != origin_identifiers.end(); ++iter) {
      GURL origin = storage::GetOriginFromIdentifier(*iter);
      origins_ptr->insert(origin);
    }
  }
}

void GetOriginsForHostOnDBThread(
    DatabaseTracker* db_tracker,
    std::set<GURL>* origins_ptr,
    const std::string& host) {
  std::vector<std::string> origin_identifiers;
  if (db_tracker->GetAllOriginIdentifiers(&origin_identifiers)) {
    for (std::vector<std::string>::const_iterator iter =
         origin_identifiers.begin();
         iter != origin_identifiers.end(); ++iter) {
      GURL origin = storage::GetOriginFromIdentifier(*iter);
      if (host == net::GetHostOrSpecFromURL(origin))
        origins_ptr->insert(origin);
    }
  }
}

void DidGetOrigins(
    const QuotaClient::GetOriginsCallback& callback,
    std::set<GURL>* origins_ptr) {
  callback.Run(*origins_ptr);
}

void DidDeleteOriginData(base::SequencedTaskRunner* original_task_runner,
                         const QuotaClient::DeletionCallback& callback,
                         int result) {
  if (result == net::ERR_IO_PENDING) {
    // The callback will be invoked via
    // DatabaseTracker::ScheduleDatabasesForDeletion.
    return;
  }

  blink::mojom::QuotaStatusCode status;
  if (result == net::OK)
    status = blink::mojom::QuotaStatusCode::kOk;
  else
    status = blink::mojom::QuotaStatusCode::kUnknown;

  original_task_runner->PostTask(FROM_HERE, base::BindOnce(callback, status));
}

}  // namespace

DatabaseQuotaClient::DatabaseQuotaClient(
    scoped_refptr<DatabaseTracker> db_tracker)
    : db_tracker_(std::move(db_tracker)) {}

DatabaseQuotaClient::~DatabaseQuotaClient() {
  if (!db_tracker_->task_runner()->RunsTasksInCurrentSequence()) {
    DatabaseTracker* tracker = db_tracker_.get();
    tracker->AddRef();
    db_tracker_ = nullptr;
    if (!tracker->task_runner()->ReleaseSoon(FROM_HERE, tracker))
      tracker->Release();
  }
}

QuotaClient::ID DatabaseQuotaClient::id() const {
  return kDatabase;
}

void DatabaseQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void DatabaseQuotaClient::GetOriginUsage(const GURL& origin_url,
                                         StorageType type,
                                         const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now.
  if (type != StorageType::kTemporary) {
    callback.Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      db_tracker_->task_runner(), FROM_HERE,
      base::BindOnce(&GetOriginUsageOnDBThread, base::RetainedRef(db_tracker_),
                     origin_url),
      static_cast<base::OnceCallback<void(int64_t usage)>>(callback));
}

void DatabaseQuotaClient::GetOriginsForType(
    StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now.
  if (type != StorageType::kTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  db_tracker_->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsOnDBThread, base::RetainedRef(db_tracker_),
                     base::Unretained(origins_ptr)),
      base::BindOnce(&DidGetOrigins, callback, base::Owned(origins_ptr)));
}

void DatabaseQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now.
  if (type != StorageType::kTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  db_tracker_->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetOriginsForHostOnDBThread,
                     base::RetainedRef(db_tracker_),
                     base::Unretained(origins_ptr), host),
      base::BindOnce(&DidGetOrigins, callback, base::Owned(origins_ptr)));
}

void DatabaseQuotaClient::DeleteOriginData(const GURL& origin,
                                           StorageType type,
                                           const DeletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(db_tracker_.get());

  // All databases are in the temp namespace for now, so nothing to delete.
  if (type != StorageType::kTemporary) {
    callback.Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  // DidDeleteOriginData() translates the net::Error response to a
  // blink::mojom::QuotaStatusCode if necessary, and no-ops as appropriate if
  // DatabaseTracker::ScheduleDatabasesForDeletion will also invoke the
  // callback.
  auto delete_callback = base::BindRepeating(
      &DidDeleteOriginData,
      base::RetainedRef(base::SequencedTaskRunnerHandle::Get()), callback);

  base::PostTaskAndReplyWithResult(
      db_tracker_->task_runner(), FROM_HERE,
      base::BindOnce(&DatabaseTracker::DeleteDataForOrigin, db_tracker_,
                     storage::GetIdentifierFromOrigin(origin), delete_callback),
      static_cast<base::OnceCallback<void(int)>>(delete_callback));
}

bool DatabaseQuotaClient::DoesSupport(StorageType type) const {
  return type == StorageType::kTemporary;
}

}  // namespace storage
