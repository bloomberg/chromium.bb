// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/testing/mock_storage_area.h"

#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MockStorageArea::MockStorageArea() = default;
MockStorageArea::~MockStorageArea() = default;

mojom::blink::StorageAreaPtr MockStorageArea::GetInterfacePtr() {
  mojom::blink::StorageAreaPtr result;
  bindings_.AddBinding(this, MakeRequest(&result));
  return result;
}

mojom::blink::StorageAreaAssociatedPtr
MockStorageArea::GetAssociatedInterfacePtr() {
  mojom::blink::StorageAreaAssociatedPtr result;
  associated_bindings_.AddBinding(
      this, MakeRequestAssociatedWithDedicatedPipe(&result));
  return result;
}

void MockStorageArea::AddObserver(
    mojom::blink::StorageAreaObserverAssociatedPtrInfo observer) {
  ++observer_count_;
}

void MockStorageArea::Put(
    const Vector<uint8_t>& key,
    const Vector<uint8_t>& value,
    const base::Optional<Vector<uint8_t>>& client_old_value,
    const String& source,
    PutCallback callback) {
  observed_put_ = true;
  observed_key_ = key;
  observed_value_ = value;
  observed_source_ = source;
  pending_callbacks_.push_back(std::move(callback));
}

void MockStorageArea::Delete(
    const Vector<uint8_t>& key,
    const base::Optional<Vector<uint8_t>>& client_old_value,
    const String& source,
    DeleteCallback callback) {
  observed_delete_ = true;
  observed_key_ = key;
  observed_source_ = source;
  pending_callbacks_.push_back(std::move(callback));
}

void MockStorageArea::DeleteAll(const String& source,
                                DeleteAllCallback callback) {
  observed_delete_all_ = true;
  observed_source_ = source;
  pending_callbacks_.push_back(std::move(callback));
}

void MockStorageArea::Get(const Vector<uint8_t>& key, GetCallback callback) {
  NOTREACHED();
}

void MockStorageArea::GetAll(
    mojom::blink::StorageAreaGetAllCallbackAssociatedPtrInfo complete_callback,
    GetAllCallback callback) {
  mojom::blink::StorageAreaGetAllCallbackAssociatedPtr complete_ptr;
  complete_ptr.Bind(std::move(complete_callback));
  pending_callbacks_.push_back(
      WTF::Bind(&mojom::blink::StorageAreaGetAllCallback::Complete,
                std::move(complete_ptr)));

  observed_get_all_ = true;
  std::move(callback).Run(true, std::move(get_all_return_values_));
}

}  // namespace blink
