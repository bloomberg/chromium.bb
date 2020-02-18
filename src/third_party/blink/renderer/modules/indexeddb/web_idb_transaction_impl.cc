// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction_impl.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBTransactionImpl::WebIDBTransactionImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int64_t transaction_id)
    : task_runner_(task_runner), transaction_id_(transaction_id) {}

WebIDBTransactionImpl::~WebIDBTransactionImpl() = default;

mojom::blink::IDBTransactionAssociatedRequest
WebIDBTransactionImpl::CreateRequest() {
  return mojo::MakeRequest(&transaction_, task_runner_);
}

void WebIDBTransactionImpl::CreateObjectStore(int64_t object_store_id,
                                              const String& name,
                                              const IDBKeyPath& key_path,
                                              bool auto_increment) {
  transaction_->CreateObjectStore(object_store_id, name, key_path,
                                  auto_increment);
}

void WebIDBTransactionImpl::DeleteObjectStore(int64_t object_store_id) {
  transaction_->DeleteObjectStore(object_store_id);
}

void WebIDBTransactionImpl::Put(int64_t object_store_id,
                                std::unique_ptr<IDBValue> value,
                                std::unique_ptr<IDBKey> primary_key,
                                mojom::IDBPutMode put_mode,
                                WebIDBCallbacks* callbacks,
                                Vector<IDBIndexKeys> index_keys) {
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, nullptr);

  size_t index_keys_size = 0;
  for (const auto& index_key : index_keys) {
    index_keys_size++;  // Account for index_key.first (int64_t).
    for (const auto& key : index_key.keys) {
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

  callbacks->SetState(nullptr, transaction_id_);
  transaction_->Put(object_store_id, std::move(value), std::move(primary_key),
                    put_mode, std::move(index_keys),
                    GetCallbacksProxy(base::WrapUnique(callbacks)));
}

void WebIDBTransactionImpl::Commit(int64_t num_errors_handled) {
  transaction_->Commit(num_errors_handled);
}

mojom::blink::IDBCallbacksAssociatedPtrInfo
WebIDBTransactionImpl::GetCallbacksProxy(
    std::unique_ptr<WebIDBCallbacks> callbacks) {
  mojom::blink::IDBCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request),
                                    task_runner_);
  return ptr_info;
}

}  // namespace blink
