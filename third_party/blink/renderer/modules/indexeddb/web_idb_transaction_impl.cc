// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction_impl.h"

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
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}

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

}  // namespace blink
