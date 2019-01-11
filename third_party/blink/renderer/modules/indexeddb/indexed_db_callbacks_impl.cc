// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"

#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_name_and_version.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"

namespace blink {

IndexedDBCallbacksImpl::IndexedDBCallbacksImpl(
    std::unique_ptr<WebIDBCallbacks> callbacks)
    : callbacks_(std::move(callbacks)) {}

IndexedDBCallbacksImpl::~IndexedDBCallbacksImpl() = default;

void IndexedDBCallbacksImpl::Error(int32_t code, const WTF::String& message) {
  callbacks_->Error(code, message);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessNamesAndVersionsList(
    Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions) {
  callbacks_->SuccessNamesAndVersionsList(std::move(names_and_versions));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessStringList(const Vector<String>& value) {
  callbacks_->SuccessStringList(std::move(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Blocked(int64_t existing_version) {
  callbacks_->Blocked(existing_version);
  // Not resetting |callbacks_|.  In this instance we will have to forward at
  // least one other call in the set UpgradeNeeded() / Success() /
  // Error().
}

void IndexedDBCallbacksImpl::UpgradeNeeded(
    mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
    int64_t old_version,
    mojom::IDBDataLoss data_loss,
    const String& data_loss_message,
    const IDBDatabaseMetadata& metadata) {
  callbacks_->UpgradeNeeded(std::move(database_info), old_version,
                            std::move(data_loss), data_loss_message, metadata);
  // Not resetting |callbacks_|.  In this instance we will have to forward at
  // least one other call in the set OnSuccess() / OnError().
}

void IndexedDBCallbacksImpl::SuccessDatabase(
    mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
    const IDBDatabaseMetadata& metadata) {
  callbacks_->SuccessDatabase(std::move(database_info), metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursor(
    mojom::blink::IDBCursorAssociatedPtrInfo cursor_info,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    base::Optional<std::unique_ptr<IDBValue>> optional_value) {
  callbacks_->SuccessCursor(std::move(cursor_info), std::move(key),
                            std::move(primary_key), std::move(optional_value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessValue(
    mojom::blink::IDBReturnValuePtr value) {
  callbacks_->SuccessValue(std::move(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorContinue(
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    base::Optional<std::unique_ptr<IDBValue>> optional_value) {
  callbacks_->SuccessCursorContinue(std::move(key), std::move(primary_key),
                                    std::move(optional_value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorPrefetch(
    Vector<std::unique_ptr<IDBKey>> keys,
    Vector<std::unique_ptr<IDBKey>> primary_keys,
    Vector<std::unique_ptr<IDBValue>> values) {
  callbacks_->SuccessCursorPrefetch(std::move(keys), std::move(primary_keys),
                                    std::move(values));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessArray(
    Vector<mojom::blink::IDBReturnValuePtr> values) {
  callbacks_->SuccessArray(std::move(values));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessKey(std::unique_ptr<IDBKey> key) {
  callbacks_->SuccessKey(std::move(key));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessInteger(int64_t value) {
  callbacks_->SuccessInteger(value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Success() {
  callbacks_->Success();
  callbacks_.reset();
}

}  // namespace blink
