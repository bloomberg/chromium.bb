/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_name_and_version.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseError;
using blink::WebIDBKeyPath;
using blink::WebIDBNameAndVersion;

namespace blink {

// static
std::unique_ptr<WebIDBCallbacksImpl> WebIDBCallbacksImpl::Create(
    IDBRequest* request) {
  return base::WrapUnique(new WebIDBCallbacksImpl(request));
}

WebIDBCallbacksImpl::WebIDBCallbacksImpl(IDBRequest* request)
    : request_(request) {
  probe::AsyncTaskScheduled(request_->GetExecutionContext(),
                            indexed_db_names::kIndexedDB, this);
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl() {
  if (request_) {
    probe::AsyncTaskCanceled(request_->GetExecutionContext(), this);
#if DCHECK_IS_ON()
    DCHECK_EQ(static_cast<WebIDBCallbacks*>(this), request_->WebCallbacks());
#endif  // DCHECK_IS_ON()
    request_->WebCallbacksDestroyed();
  }
}

void WebIDBCallbacksImpl::OnError(const WebIDBDatabaseError& error) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "error");
  request_->HandleResponse(DOMException::Create(
      static_cast<DOMExceptionCode>(error.Code()), error.Message()));
}

void WebIDBCallbacksImpl::OnSuccess(
    const Vector<WebIDBNameAndVersion>& name_and_version_list) {
  // Only implemented in idb_factory.cc for the promise-based databases() call.
  NOTREACHED();
}

void WebIDBCallbacksImpl::OnSuccess(const Vector<String>& string_list) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  request_->EnqueueResponse(string_list);
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBCursor* cursor,
                                    std::unique_ptr<IDBKey> key,
                                    std::unique_ptr<IDBKey> primary_key,
                                    std::unique_ptr<IDBValue> value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  value->SetIsolate(request_->GetIsolate());
  request_->HandleResponse(base::WrapUnique(cursor), std::move(key),
                           std::move(primary_key), std::move(value));
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBDatabase* backend,
                                    const IDBDatabaseMetadata& metadata) {
  std::unique_ptr<WebIDBDatabase> db = base::WrapUnique(backend);
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(), this,
                                "success");
#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    request_->EnqueueResponse(std::move(db), IDBDatabaseMetadata(metadata));
  } else if (db) {
    db->Close();
  }
}

void WebIDBCallbacksImpl::OnSuccess(std::unique_ptr<IDBKey> key) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  request_->HandleResponse(std::move(key));
}

void WebIDBCallbacksImpl::OnSuccess(std::unique_ptr<IDBValue> value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  value->SetIsolate(request_->GetIsolate());
  request_->HandleResponse(std::move(value));
}

void WebIDBCallbacksImpl::OnSuccess(Vector<std::unique_ptr<IDBValue>> values) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  for (const std::unique_ptr<IDBValue>& value : values) {
    value->SetIsolate(request_->GetIsolate());
  }
  request_->HandleResponse(std::move(values));
}

void WebIDBCallbacksImpl::OnSuccess(long long value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  request_->HandleResponse(value);
}

void WebIDBCallbacksImpl::OnSuccess() {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  request_->HandleResponse();
}

void WebIDBCallbacksImpl::OnSuccess(std::unique_ptr<IDBKey> key,
                                    std::unique_ptr<IDBKey> primary_key,
                                    std::unique_ptr<IDBValue> value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  value->SetIsolate(request_->GetIsolate());
  request_->HandleResponse(std::move(key), std::move(primary_key),
                           std::move(value));
}

void WebIDBCallbacksImpl::OnBlocked(long long old_version) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "blocked");
#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  request_->EnqueueBlocked(old_version);
}

void WebIDBCallbacksImpl::OnUpgradeNeeded(long long old_version,
                                          WebIDBDatabase* database,
                                          const IDBDatabaseMetadata& metadata,
                                          mojom::IDBDataLoss data_loss,
                                          String data_loss_message) {
  std::unique_ptr<WebIDBDatabase> db = base::WrapUnique(database);
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(), this,
                                "upgradeNeeded");
#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    request_->EnqueueUpgradeNeeded(old_version, std::move(db),
                                   IDBDatabaseMetadata(metadata), data_loss,
                                   data_loss_message);
  } else {
    db->Close();
  }
}

void WebIDBCallbacksImpl::Detach() {
  request_.Clear();
}

}  // namespace blink
