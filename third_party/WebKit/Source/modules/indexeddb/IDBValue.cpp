// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBValue.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "platform/blob/BlobData.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"
#include "v8/include/v8.h"

namespace blink {

IDBValue::IDBValue(const WebIDBValue& value, v8::Isolate* isolate)
    : IDBValue(value.data,
               value.web_blob_info,
               value.primary_key,
               value.key_path) {
  isolate_ = isolate;
  external_allocated_size_ = data_ ? static_cast<int64_t>(data_->size()) : 0l;
  if (external_allocated_size_)
    isolate_->AdjustAmountOfExternalAllocatedMemory(external_allocated_size_);
}

IDBValue::IDBValue(scoped_refptr<SharedBuffer> data,
                   const WebVector<WebBlobInfo>& web_blob_info,
                   IDBKey* primary_key,
                   const IDBKeyPath& key_path)
    : data_(std::move(data)),
      primary_key_(primary_key && primary_key->IsValid() ? primary_key
                                                         : nullptr),
      key_path_(key_path) {
  blob_info_.ReserveInitialCapacity(web_blob_info.size());
  blob_data_.ReserveInitialCapacity(web_blob_info.size());

  for (const WebBlobInfo& info : web_blob_info) {
    blob_info_.push_back(info);
    blob_data_.push_back(
        BlobDataHandle::Create(info.Uuid(), info.GetType(), info.size()));
  }
}

IDBValue::IDBValue(std::unique_ptr<IDBValue> value,
                   IDBKey* primary_key,
                   const IDBKeyPath& key_path)
    : data_(std::move(value->data_)),
      blob_data_(std::move(value->blob_data_)),
      blob_info_(std::move(value->blob_info_)),
      primary_key_(primary_key),
      key_path_(key_path) {}

IDBValue::IDBValue(scoped_refptr<SharedBuffer> unwrapped_data,
                   Vector<scoped_refptr<BlobDataHandle>> blob_data,
                   Vector<WebBlobInfo> blob_info,
                   const IDBKey* primary_key,
                   const IDBKeyPath& key_path)
    : data_(std::move(unwrapped_data)),
      blob_data_(std::move(blob_data)),
      blob_info_(std::move(blob_info)),
      primary_key_(primary_key),
      key_path_(key_path) {}

IDBValue::~IDBValue() {
  if (isolate_)
    isolate_->AdjustAmountOfExternalAllocatedMemory(-external_allocated_size_);
}

std::unique_ptr<IDBValue> IDBValue::Create(const WebIDBValue& value,
                                           v8::Isolate* isolate) {
  return base::WrapUnique(new IDBValue(value, isolate));
}

std::unique_ptr<IDBValue> IDBValue::Create(std::unique_ptr<IDBValue> value,
                                           IDBKey* primary_key,
                                           const IDBKeyPath& key_path) {
  return base::WrapUnique(
      new IDBValue(std::move(value), primary_key, key_path));
}

std::unique_ptr<IDBValue> IDBValue::Create(
    scoped_refptr<SharedBuffer> unwrapped_data,
    Vector<scoped_refptr<BlobDataHandle>> blob_data,
    Vector<WebBlobInfo> blob_info,
    const IDBKey* primary_key,
    const IDBKeyPath& key_path) {
  return base::WrapUnique(
      new IDBValue(std::move(unwrapped_data), std::move(blob_data),
                   std::move(blob_info), primary_key, key_path));
}

Vector<String> IDBValue::GetUUIDs() const {
  Vector<String> uuids;
  uuids.ReserveCapacity(blob_info_.size());
  for (const WebBlobInfo& info : blob_info_)
    uuids.push_back(info.Uuid());
  return uuids;
}

scoped_refptr<SerializedScriptValue> IDBValue::CreateSerializedValue() const {
  return SerializedScriptValue::Create(data_);
}

bool IDBValue::IsNull() const {
  return !data_.get();
}

}  // namespace blink
