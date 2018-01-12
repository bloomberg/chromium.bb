// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBTestHelper.h"

#include <memory>
#include <utility>

#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBValueWrapping.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebBlobInfo.h"

namespace blink {

std::unique_ptr<IDBValue> CreateNullIDBValueForTesting(v8::Isolate* isolate) {
  scoped_refptr<SerializedScriptValue> null_ssv =
      SerializedScriptValue::NullValue();

  base::span<const uint8_t> ssv_wire_bytes = null_ssv->GetWireData();

  scoped_refptr<SharedBuffer> idb_value_buffer = SharedBuffer::Create();
  idb_value_buffer->Append(reinterpret_cast<const char*>(ssv_wire_bytes.data()),
                           ssv_wire_bytes.length());
  std::unique_ptr<IDBValue> idb_value = IDBValue::Create(
      std::move(idb_value_buffer), Vector<scoped_refptr<BlobDataHandle>>(),
      Vector<WebBlobInfo>());
  idb_value->SetInjectedPrimaryKey(IDBKey::CreateNumber(42.0),
                                   IDBKeyPath(String("primaryKey")));
  idb_value->SetIsolate(isolate);
  return idb_value;
}

std::unique_ptr<IDBValue> CreateIDBValueForTesting(v8::Isolate* isolate,
                                                   bool create_wrapped_value) {
  size_t element_count = create_wrapped_value ? 16 : 2;
  v8::Local<v8::Array> v8_array = v8::Array::New(isolate, element_count);
  for (size_t i = 0; i < element_count; ++i)
    v8_array->Set(i, v8::True(isolate));

  NonThrowableExceptionState non_throwable_exception_state;
  IDBValueWrapper wrapper(isolate, v8_array,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.DoneCloning();
  wrapper.WrapIfBiggerThan(create_wrapped_value ? 0 : 1024 * element_count);

  Vector<scoped_refptr<BlobDataHandle>> blob_data_handles =
      wrapper.TakeBlobDataHandles();
  Vector<WebBlobInfo> blob_infos = wrapper.TakeBlobInfo();
  scoped_refptr<SharedBuffer> wrapped_marker_buffer = wrapper.TakeWireBytes();

  std::unique_ptr<IDBValue> idb_value =
      IDBValue::Create(std::move(wrapped_marker_buffer),
                       std::move(blob_data_handles), std::move(blob_infos));
  idb_value->SetInjectedPrimaryKey(IDBKey::CreateNumber(42.0),
                                   IDBKeyPath(String("primaryKey")));
  idb_value->SetIsolate(isolate);

  DCHECK_EQ(create_wrapped_value,
            IDBValueUnwrapper::IsWrapped(idb_value.get()));
  return idb_value;
}

}  // namespace blink
