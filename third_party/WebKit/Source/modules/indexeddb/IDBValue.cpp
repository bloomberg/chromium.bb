// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBValue.h"

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"
#include "v8/include/v8.h"

namespace blink {

IDBValue::IDBValue() = default;

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
      blob_data_(WTF::MakeUnique<Vector<scoped_refptr<BlobDataHandle>>>()),
      blob_info_(
          WTF::WrapUnique(new Vector<WebBlobInfo>(web_blob_info.size()))),
      primary_key_(primary_key && primary_key->IsValid() ? primary_key
                                                         : nullptr),
      key_path_(key_path) {
  for (size_t i = 0; i < web_blob_info.size(); ++i) {
    const WebBlobInfo& info = (*blob_info_)[i] = web_blob_info[i];
    blob_data_->push_back(
        BlobDataHandle::Create(info.Uuid(), info.GetType(), info.size()));
  }
}

IDBValue::IDBValue(const IDBValue* value,
                   IDBKey* primary_key,
                   const IDBKeyPath& key_path)
    : data_(value->data_),
      blob_data_(WTF::MakeUnique<Vector<scoped_refptr<BlobDataHandle>>>()),
      blob_info_(
          WTF::WrapUnique(new Vector<WebBlobInfo>(value->blob_info_->size()))),
      primary_key_(primary_key),
      key_path_(key_path) {
  for (size_t i = 0; i < value->blob_info_->size(); ++i) {
    const WebBlobInfo& info = (*blob_info_)[i] = value->blob_info_->at(i);
    blob_data_->push_back(
        BlobDataHandle::Create(info.Uuid(), info.GetType(), info.size()));
  }
}

IDBValue::IDBValue(
    scoped_refptr<SharedBuffer> unwrapped_data,
    std::unique_ptr<Vector<scoped_refptr<BlobDataHandle>>> blob_data,
    std::unique_ptr<Vector<WebBlobInfo>> blob_info,
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

scoped_refptr<IDBValue> IDBValue::Create() {
  return WTF::AdoptRef(new IDBValue());
}

scoped_refptr<IDBValue> IDBValue::Create(const WebIDBValue& value,
                                         v8::Isolate* isolate) {
  return WTF::AdoptRef(new IDBValue(value, isolate));
}

scoped_refptr<IDBValue> IDBValue::Create(const IDBValue* value,
                                         IDBKey* primary_key,
                                         const IDBKeyPath& key_path) {
  return WTF::AdoptRef(new IDBValue(value, primary_key, key_path));
}

scoped_refptr<IDBValue> IDBValue::Create(
    scoped_refptr<SharedBuffer> unwrapped_data,
    std::unique_ptr<Vector<scoped_refptr<BlobDataHandle>>> blob_data,
    std::unique_ptr<Vector<WebBlobInfo>> blob_info,
    const IDBKey* primary_key,
    const IDBKeyPath& key_path) {
  return WTF::AdoptRef(new IDBValue(std::move(unwrapped_data),
                                    std::move(blob_data), std::move(blob_info),
                                    primary_key, key_path));
}

Vector<String> IDBValue::GetUUIDs() const {
  Vector<String> uuids;
  uuids.ReserveCapacity(blob_info_->size());
  for (const auto& info : *blob_info_)
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
