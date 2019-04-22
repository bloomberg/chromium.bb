// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/renderer/dom_storage/mock_leveldb_wrapper.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"

namespace content {

class MockLevelDBWrapper::MockSessionStorageNamespace
    : public blink::mojom::SessionStorageNamespace {
 public:
  MockSessionStorageNamespace(std::string namespace_id,
                              MockLevelDBWrapper* wrapper)
      : namespace_id_(std::move(namespace_id)), wrapper_(wrapper) {}

  void OpenArea(const url::Origin& origin,
                blink::mojom::StorageAreaAssociatedRequest database) override {
    bindings_.AddBinding(wrapper_, std::move(database));
  }

  void Clone(const std::string& clone_to_namespace) override {
    wrapper_->observed_clone_ = true;
    wrapper_->observed_clone_from_namespace_ = namespace_id_;
    wrapper_->observed_clone_to_namespace_ = clone_to_namespace;
  }

 private:
  std::string namespace_id_;
  MockLevelDBWrapper* wrapper_;
  mojo::AssociatedBindingSet<blink::mojom::StorageArea> bindings_;
};

MockLevelDBWrapper::MockLevelDBWrapper() {}
MockLevelDBWrapper::~MockLevelDBWrapper() {}

void MockLevelDBWrapper::OpenLocalStorage(
    const url::Origin& origin,
    blink::mojom::StorageAreaRequest database) {
  bindings_.AddBinding(this, std::move(database));
}

void MockLevelDBWrapper::OpenSessionStorage(
    const std::string& namespace_id,
    blink::mojom::SessionStorageNamespaceRequest request) {
  namespace_bindings_.AddBinding(
      std::make_unique<MockSessionStorageNamespace>(namespace_id, this),
      std::move(request));
}

void MockLevelDBWrapper::AddObserver(
    blink::mojom::StorageAreaObserverAssociatedPtrInfo observer) {}

void MockLevelDBWrapper::Put(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& value,
    const base::Optional<std::vector<uint8_t>>& client_old_value,
    const std::string& source,
    PutCallback callback) {
  observed_put_ = true;
  observed_key_ = key;
  observed_value_ = value;
  observed_source_ = source;
  pending_callbacks_.push_back(std::move(callback));
}

void MockLevelDBWrapper::Delete(
    const std::vector<uint8_t>& key,
    const base::Optional<std::vector<uint8_t>>& client_old_value,
    const std::string& source,
    DeleteCallback callback) {
  observed_delete_ = true;
  observed_key_ = key;
  observed_source_ = source;
  pending_callbacks_.push_back(std::move(callback));
}

void MockLevelDBWrapper::DeleteAll(const std::string& source,
                                   DeleteAllCallback callback) {
  observed_delete_all_ = true;
  observed_source_ = source;
  pending_callbacks_.push_back(std::move(callback));
}

void MockLevelDBWrapper::Get(const std::vector<uint8_t>& key,
                             GetCallback callback) {}

void MockLevelDBWrapper::GetAll(
    blink::mojom::StorageAreaGetAllCallbackAssociatedPtrInfo complete_callback,
    GetAllCallback callback) {
  blink::mojom::StorageAreaGetAllCallbackAssociatedPtr complete_ptr;
  complete_ptr.Bind(std::move(complete_callback));
  pending_callbacks_.push_back(
      base::BindOnce(&blink::mojom::StorageAreaGetAllCallback::Complete,
                     std::move(complete_ptr)));

  observed_get_all_ = true;
  std::vector<blink::mojom::KeyValuePtr> all;
  for (const auto& it : get_all_return_values_) {
    auto kv = blink::mojom::KeyValue::New();
    kv->key = it.first;
    kv->value = it.second;
    all.push_back(std::move(kv));
  }
  std::move(callback).Run(true, std::move(all));
}

}  // namespace content
