// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"

#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_name_and_version.h"

using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBNameAndVersion;
using blink::mojom::blink::IDBDatabaseAssociatedPtrInfo;

namespace blink {

namespace {

std::unique_ptr<IDBValue> ConvertReturnValue(
    const mojom::blink::IDBReturnValuePtr& input) {
  if (!input) {
    return IDBValue::Create(scoped_refptr<SharedBuffer>(),
                            Vector<WebBlobInfo>());
  }

  std::unique_ptr<IDBValue> output = std::move(input->value);
  output->SetInjectedPrimaryKey(std::move(input->primary_key), input->key_path);
  return output;
}

WebIDBNameAndVersion ConvertNameVersion(
    const mojom::blink::IDBNameAndVersionPtr& name_and_version) {
  return WebIDBNameAndVersion(name_and_version->name,
                              name_and_version->version);
}

}  // namespace

IndexedDBCallbacksImpl::IndexedDBCallbacksImpl(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    int64_t transaction_id,
    const base::WeakPtr<WebIDBCursorImpl>& cursor)
    : callbacks_(std::move(callbacks)),
      cursor_(cursor),
      transaction_id_(transaction_id) {}

IndexedDBCallbacksImpl::~IndexedDBCallbacksImpl() = default;

void IndexedDBCallbacksImpl::Error(int32_t code, const WTF::String& message) {
  callbacks_->OnError(WebIDBDatabaseError(code, String(message)));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessNamesAndVersionsList(
    Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions) {
  Vector<WebIDBNameAndVersion> output_names_and_versions;
  output_names_and_versions.ReserveInitialCapacity(names_and_versions.size());
  for (const mojom::blink::IDBNameAndVersionPtr& name_version :
       names_and_versions)
    output_names_and_versions.emplace_back(ConvertNameVersion(name_version));
  callbacks_->OnSuccess(output_names_and_versions);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessStringList(const Vector<String>& value) {
  Vector<String> values(value.size());
  std::transform(value.begin(), value.end(), values.begin(),
                 [](const WTF::String& s) { return String(s); });
  callbacks_->OnSuccess(values);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Blocked(int64_t existing_version) {
  callbacks_->OnBlocked(existing_version);
  // Not resetting |callbacks_|.  In this instance we will have to forward at
  // least one other call in the set OnUpgradeNeeded() / OnSuccess() /
  // OnError().
}

void IndexedDBCallbacksImpl::UpgradeNeeded(
    IDBDatabaseAssociatedPtrInfo database_info,
    int64_t old_version,
    mojom::IDBDataLoss data_loss,
    const String& data_loss_message,
    const IDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database = new WebIDBDatabaseImpl(std::move(database_info));
  callbacks_->OnUpgradeNeeded(old_version, database, metadata, data_loss,
                              String(data_loss_message));
  // Not resetting |callbacks_|.  In this instance we will have to forward at
  // least one other call in the set OnSuccess() / OnError().
}

void IndexedDBCallbacksImpl::SuccessDatabase(
    IDBDatabaseAssociatedPtrInfo database_info,
    const IDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database = nullptr;
  if (database_info.is_valid())
    database = new WebIDBDatabaseImpl(std::move(database_info));

  callbacks_->OnSuccess(database, metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursor(
    mojom::blink::IDBCursorAssociatedPtrInfo cursor_info,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    base::Optional<std::unique_ptr<IDBValue>> optional_value) {
  WebIDBCursorImpl* cursor =
      new WebIDBCursorImpl(std::move(cursor_info), transaction_id_);
  std::unique_ptr<IDBValue> value;
  if (optional_value.has_value()) {
    value = std::move(optional_value.value());
  } else {
    value =
        IDBValue::Create(scoped_refptr<SharedBuffer>(), Vector<WebBlobInfo>());
  }
  DCHECK(value);
  callbacks_->OnSuccess(cursor, std::move(key), std::move(primary_key),
                        std::move(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessValue(
    mojom::blink::IDBReturnValuePtr value) {
  callbacks_->OnSuccess(ConvertReturnValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorContinue(
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    base::Optional<std::unique_ptr<IDBValue>> optional_value) {
  std::unique_ptr<IDBValue> value;
  if (optional_value.has_value()) {
    value = std::move(optional_value.value());
  } else {
    value =
        IDBValue::Create(scoped_refptr<SharedBuffer>(), Vector<WebBlobInfo>());
  }
  DCHECK(value);
  callbacks_->OnSuccess(std::move(key), std::move(primary_key),
                        std::move(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorPrefetch(
    Vector<std::unique_ptr<IDBKey>> keys,
    Vector<std::unique_ptr<IDBKey>> primary_keys,
    Vector<std::unique_ptr<IDBValue>> values) {
  if (cursor_) {
    cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                             std::move(values));
    cursor_->CachedContinue(callbacks_.get());
  }
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessArray(
    Vector<mojom::blink::IDBReturnValuePtr> values) {
  Vector<std::unique_ptr<IDBValue>> idb_values;
  idb_values.ReserveInitialCapacity(values.size());
  for (const mojom::blink::IDBReturnValuePtr& value : values)
    idb_values.emplace_back(ConvertReturnValue(value));
  callbacks_->OnSuccess(std::move(idb_values));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessKey(std::unique_ptr<IDBKey> key) {
  callbacks_->OnSuccess(std::move(key));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessInteger(int64_t value) {
  callbacks_->OnSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Success() {
  callbacks_->OnSuccess();
  callbacks_.reset();
}

}  // namespace blink
