// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"

#include "base/format_macros.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(
    mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  database_.Bind(std::move(database_info), task_runner_);
}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() = default;

void WebIDBDatabaseImpl::CreateObjectStore(int64_t transaction_id,
                                           int64_t object_store_id,
                                           const String& name,
                                           const IDBKeyPath& key_path,
                                           bool auto_increment) {
  database_->CreateObjectStore(transaction_id, object_store_id, name, key_path,
                               auto_increment);
}

void WebIDBDatabaseImpl::DeleteObjectStore(int64_t transaction_id,
                                           int64_t object_store_id) {
  database_->DeleteObjectStore(transaction_id, object_store_id);
}

void WebIDBDatabaseImpl::RenameObjectStore(int64_t transaction_id,
                                           int64_t object_store_id,
                                           const String& new_name) {
  database_->RenameObjectStore(transaction_id, object_store_id, new_name);
}

void WebIDBDatabaseImpl::CreateTransaction(
    int64_t transaction_id,
    const Vector<int64_t>& object_store_ids,
    mojom::IDBTransactionMode mode) {
  database_->CreateTransaction(transaction_id, object_store_ids, mode);
}

void WebIDBDatabaseImpl::Close() {
  database_->Close();
}

void WebIDBDatabaseImpl::VersionChangeIgnored() {
  database_->VersionChangeIgnored();
}

void WebIDBDatabaseImpl::AddObserver(
    int64_t transaction_id,
    int32_t observer_id,
    bool include_transaction,
    bool no_records,
    bool values,
    std::bitset<blink::kIDBOperationTypeCount> operation_types) {
  static_assert(kIDBOperationTypeCount < sizeof(uint32_t) * CHAR_BIT,
                "IDBOperationTypeCount exceeds size of uint32_t");
  database_->AddObserver(transaction_id, observer_id, include_transaction,
                         no_records, values,
                         static_cast<uint32_t>(operation_types.to_ulong()));
}

void WebIDBDatabaseImpl::RemoveObservers(const Vector<int32_t>& observer_ids) {
  database_->RemoveObservers(observer_ids);
}

void WebIDBDatabaseImpl::Get(int64_t transaction_id,
                             int64_t object_store_id,
                             int64_t index_id,
                             const IDBKeyRange* key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->Get(transaction_id, object_store_id, index_id,
                 std::move(key_range_ptr), key_only,
                 GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::GetAll(int64_t transaction_id,
                                int64_t object_store_id,
                                int64_t index_id,
                                const IDBKeyRange* key_range,
                                int64_t max_count,
                                bool key_only,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->GetAll(transaction_id, object_store_id, index_id,
                    std::move(key_range_ptr), key_only, max_count,
                    GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::Put(int64_t transaction_id,
                             int64_t object_store_id,
                             std::unique_ptr<IDBValue> value,
                             std::unique_ptr<IDBKey> primary_key,
                             mojom::IDBPutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             Vector<IDBIndexKeys> index_keys) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  size_t index_keys_size = 0;
  for (const auto& index_key : index_keys) {
    index_keys_size++;  // Account for index_key.first (int64_t).
    for (const auto& key : index_key.second) {
      index_keys_size += key->SizeEstimate();
    }
  }

  size_t arg_size =
      value->DataSize() + primary_key->SizeEstimate() + index_keys_size;
  if (arg_size >= max_put_value_size_) {
    callbacks->Error(
        blink::kWebIDBDatabaseExceptionUnknownError,
        String::Format("The serialized keys and/or value are too large"
                       " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
                       arg_size, max_put_value_size_));
    return;
  }

  callbacks->SetState(nullptr, transaction_id);
  database_->Put(transaction_id, object_store_id, std::move(value),
                 std::move(primary_key), put_mode, std::move(index_keys),
                 GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::SetIndexKeys(int64_t transaction_id,
                                      int64_t object_store_id,
                                      std::unique_ptr<IDBKey> primary_key,
                                      Vector<IDBIndexKeys> index_keys) {
  database_->SetIndexKeys(transaction_id, object_store_id,
                          std::move(primary_key), std::move(index_keys));
}

void WebIDBDatabaseImpl::SetIndexesReady(int64_t transaction_id,
                                         int64_t object_store_id,
                                         const Vector<int64_t>& index_ids) {
  database_->SetIndexesReady(transaction_id, object_store_id,
                             std::move(index_ids));
}

void WebIDBDatabaseImpl::OpenCursor(int64_t transaction_id,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const IDBKeyRange* key_range,
                                    mojom::IDBCursorDirection direction,
                                    bool key_only,
                                    mojom::IDBTaskType task_type,
                                    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->OpenCursor(transaction_id, object_store_id, index_id,
                        std::move(key_range_ptr), direction, key_only,
                        task_type,
                        GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::Count(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const IDBKeyRange* key_range,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->Count(transaction_id, object_store_id, index_id,
                   std::move(key_range_ptr),
                   GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::Delete(int64_t transaction_id,
                                int64_t object_store_id,
                                const IDBKey* primary_key,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(IDBKeyRange::Create(primary_key));
  callbacks->SetState(nullptr, transaction_id);
  database_->DeleteRange(transaction_id, object_store_id,
                         std::move(key_range_ptr),
                         GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::DeleteRange(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const IDBKeyRange* key_range,
                                     WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  callbacks->SetState(nullptr, transaction_id);
  database_->DeleteRange(transaction_id, object_store_id,
                         std::move(key_range_ptr),
                         GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    WebIDBCallbacks* callbacks) {
  callbacks->SetState(nullptr, transaction_id);
  database_->GetKeyGeneratorCurrentNumber(
      transaction_id, object_store_id,
      GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::Clear(int64_t transaction_id,
                               int64_t object_store_id,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id, nullptr);

  callbacks->SetState(nullptr, transaction_id);
  database_->Clear(transaction_id, object_store_id,
                   GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBDatabaseImpl::CreateIndex(int64_t transaction_id,
                                     int64_t object_store_id,
                                     int64_t index_id,
                                     const String& name,
                                     const IDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  database_->CreateIndex(transaction_id, object_store_id, index_id, name,
                         key_path, unique, multi_entry);
}

void WebIDBDatabaseImpl::DeleteIndex(int64_t transaction_id,
                                     int64_t object_store_id,
                                     int64_t index_id) {
  database_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void WebIDBDatabaseImpl::RenameIndex(int64_t transaction_id,
                                     int64_t object_store_id,
                                     int64_t index_id,
                                     const String& new_name) {
  DCHECK(!new_name.IsNull());
  database_->RenameIndex(transaction_id, object_store_id, index_id, new_name);
}

void WebIDBDatabaseImpl::Abort(int64_t transaction_id) {
  database_->Abort(transaction_id);
}

void WebIDBDatabaseImpl::Commit(int64_t transaction_id,
                                int64_t num_errors_handled) {
  database_->Commit(transaction_id, num_errors_handled);
}

mojom::blink::IDBCallbacksAssociatedPtrInfo
WebIDBDatabaseImpl::GetCallbacksProxy(
    std::unique_ptr<WebIDBCallbacks> callbacks) {
  mojom::blink::IDBCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request),
                                    task_runner_);
  return ptr_info;
}

}  // namespace blink
