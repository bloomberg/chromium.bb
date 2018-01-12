/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBDatabaseCallbacks.h"

#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/WebIDBDatabaseCallbacksImpl.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBObservation.h"

namespace blink {

IDBDatabaseCallbacks* IDBDatabaseCallbacks::Create() {
  return new IDBDatabaseCallbacks();
}

IDBDatabaseCallbacks::IDBDatabaseCallbacks() : database_(nullptr) {}

IDBDatabaseCallbacks::~IDBDatabaseCallbacks() = default;

void IDBDatabaseCallbacks::Trace(blink::Visitor* visitor) {
  visitor->Trace(database_);
}

void IDBDatabaseCallbacks::OnForcedClose() {
  if (database_)
    database_->ForceClose();
}

void IDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                           int64_t new_version) {
  if (database_)
    database_->OnVersionChange(old_version, new_version);
}

void IDBDatabaseCallbacks::OnAbort(int64_t transaction_id,
                                   DOMException* error) {
  if (database_)
    database_->OnAbort(transaction_id, error);
}

void IDBDatabaseCallbacks::OnComplete(int64_t transaction_id) {
  if (database_)
    database_->OnComplete(transaction_id);
}

void IDBDatabaseCallbacks::OnChanges(
    const WebIDBDatabaseCallbacks::ObservationIndexMap& observation_index_map,
    WebVector<WebIDBObservation> observations,
    const WebIDBDatabaseCallbacks::TransactionMap& transactions) {
  if (!database_)
    return;

  database_->OnChanges(observation_index_map, std::move(observations),
                       transactions);
}

void IDBDatabaseCallbacks::Connect(IDBDatabase* database) {
  DCHECK(!database_);
  DCHECK(database);
  database_ = database;
}

std::unique_ptr<WebIDBDatabaseCallbacks>
IDBDatabaseCallbacks::CreateWebCallbacks() {
  DCHECK(!web_callbacks_);
  std::unique_ptr<WebIDBDatabaseCallbacks> callbacks =
      WebIDBDatabaseCallbacksImpl::Create(this);
  web_callbacks_ = callbacks.get();
  return callbacks;
}

void IDBDatabaseCallbacks::DetachWebCallbacks() {
  if (web_callbacks_) {
    web_callbacks_->Detach();
    web_callbacks_ = nullptr;
  }
}

void IDBDatabaseCallbacks::WebCallbacksDestroyed() {
  DCHECK(web_callbacks_);
  web_callbacks_ = nullptr;
}

}  // namespace blink
