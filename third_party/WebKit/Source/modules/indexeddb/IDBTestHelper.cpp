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

std::unique_ptr<IDBValue> CreateNullIDBValueForTesting() {
  scoped_refptr<SerializedScriptValue> null_ssv =
      SerializedScriptValue::NullValue();

  StringView ssv_wire_bytes = null_ssv->GetWireData();
  DCHECK(ssv_wire_bytes.Is8Bit());

  scoped_refptr<SharedBuffer> idb_value_buffer = SharedBuffer::Create();
  idb_value_buffer->Append(
      reinterpret_cast<const char*>(ssv_wire_bytes.Characters8()),
      ssv_wire_bytes.length());
  return IDBValue::Create(std::move(idb_value_buffer),
                          Vector<scoped_refptr<BlobDataHandle>>(),
                          Vector<WebBlobInfo>(), IDBKey::CreateNumber(42.0),
                          IDBKeyPath(String("primaryKey")));
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
  IDBKey* key = IDBKey::CreateNumber(42.0);
  IDBKeyPath key_path(String("primaryKey"));

  std::unique_ptr<IDBValue> idb_value = IDBValue::Create(
      std::move(wrapped_marker_buffer), std::move(blob_data_handles),
      std::move(blob_infos), key, key_path);

  DCHECK_EQ(create_wrapped_value,
            IDBValueUnwrapper::IsWrapped(idb_value.get()));
  return idb_value;
}

}  // namespace blink
