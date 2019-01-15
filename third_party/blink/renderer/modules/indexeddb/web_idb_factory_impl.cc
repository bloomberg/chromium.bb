// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBFactoryImpl::WebIDBFactoryImpl(
    mojom::blink::IDBFactoryPtrInfo factory_info)
    : factory_(std::move(factory_info)) {}

WebIDBFactoryImpl::~WebIDBFactoryImpl() = default;

void WebIDBFactoryImpl::GetDatabaseInfo(
    WebIDBCallbacks* callbacks,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  factory_->GetDatabaseInfo(
      GetCallbacksProxy(base::WrapUnique(callbacks), std::move(task_runner)));
}

void WebIDBFactoryImpl::GetDatabaseNames(
    WebIDBCallbacks* callbacks,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  factory_->GetDatabaseNames(
      GetCallbacksProxy(base::WrapUnique(callbacks), std::move(task_runner)));
}

void WebIDBFactoryImpl::Open(
    const String& name,
    long long version,
    long long transaction_id,
    WebIDBCallbacks* callbacks,
    WebIDBDatabaseCallbacks* database_callbacks,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  auto database_callbacks_impl =
      std::make_unique<IndexedDBDatabaseCallbacksImpl>(
          base::WrapUnique(database_callbacks));
  DCHECK(!name.IsNull());
  factory_->Open(GetCallbacksProxy(base::WrapUnique(callbacks), task_runner),
                 GetDatabaseCallbacksProxy(std::move(database_callbacks_impl),
                                           task_runner),
                 name, version, transaction_id);
}

void WebIDBFactoryImpl::DeleteDatabase(
    const String& name,
    WebIDBCallbacks* callbacks,
    bool force_close,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  DCHECK(!name.IsNull());
  factory_->DeleteDatabase(
      GetCallbacksProxy(base::WrapUnique(callbacks), std::move(task_runner)),
      name, force_close);
}

mojom::blink::IDBCallbacksAssociatedPtrInfo
WebIDBFactoryImpl::GetCallbacksProxy(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojom::blink::IDBCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request),
                                    std::move(task_runner));
  return ptr_info;
}

mojom::blink::IDBDatabaseCallbacksAssociatedPtrInfo
WebIDBFactoryImpl::GetDatabaseCallbacksProxy(
    std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojom::blink::IDBDatabaseCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request),
                                    std::move(task_runner));
  return ptr_info;
}

}  // namespace blink
