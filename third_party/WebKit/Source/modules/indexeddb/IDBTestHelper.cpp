// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBTestHelper.h"

#include <utility>
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBValueWrapping.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebBlobInfo.h"

namespace blink {

RefPtr<IDBValue> CreateIDBValueForTesting(v8::Isolate* isolate,
                                          bool create_wrapped_value) {
  size_t element_count = create_wrapped_value ? 16 : 2;
  v8::Local<v8::Array> v8_array = v8::Array::New(isolate, element_count);
  for (size_t i = 0; i < element_count; ++i)
    v8_array->Set(i, v8::True(isolate));

  NonThrowableExceptionState non_throwable_exception_state;
  IDBValueWrapper wrapper(isolate, v8_array,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.WrapIfBiggerThan(create_wrapped_value ? 0 : 1024 * element_count);

  std::unique_ptr<Vector<RefPtr<BlobDataHandle>>> blob_data_handles =
      WTF::MakeUnique<Vector<RefPtr<BlobDataHandle>>>();
  wrapper.ExtractBlobDataHandles(blob_data_handles.get());
  Vector<WebBlobInfo>& blob_infos = wrapper.WrappedBlobInfo();
  RefPtr<SharedBuffer> wrapped_marker_buffer = wrapper.ExtractWireBytes();
  IDBKey* key = IDBKey::CreateNumber(42.0);
  IDBKeyPath key_path(String("primaryKey"));

  RefPtr<IDBValue> idb_value = IDBValue::Create(
      std::move(wrapped_marker_buffer), std::move(blob_data_handles),
      WTF::MakeUnique<Vector<WebBlobInfo>>(blob_infos), key, key_path);

  DCHECK_EQ(create_wrapped_value,
            IDBValueUnwrapper::IsWrapped(idb_value.Get()));
  return idb_value;
}

}  // namespace blink
