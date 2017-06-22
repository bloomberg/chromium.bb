// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBValueWrapping.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/fileapi/Blob.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

TEST(IDBValueUnwrapperTest, IsWrapped) {
  V8TestingScope scope;
  NonThrowableExceptionState non_throwable_exception_state;
  v8::Local<v8::Value> v8_true = v8::True(scope.GetIsolate());
  IDBValueWrapper wrapper(scope.GetIsolate(), v8_true,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.WrapIfBiggerThan(0);
  Vector<RefPtr<BlobDataHandle>> blob_data_handles;
  wrapper.ExtractBlobDataHandles(&blob_data_handles);
  Vector<WebBlobInfo>& blob_infos = wrapper.WrappedBlobInfo();
  RefPtr<SharedBuffer> wrapped_marker_buffer = wrapper.ExtractWireBytes();

  RefPtr<IDBValue> wrapped_value = IDBValue::Create(
      wrapped_marker_buffer,
      WTF::MakeUnique<Vector<RefPtr<BlobDataHandle>>>(blob_data_handles),
      WTF::MakeUnique<Vector<WebBlobInfo>>(blob_infos));
  EXPECT_TRUE(IDBValueUnwrapper::IsWrapped(wrapped_value.Get()));

  Vector<char> wrapped_marker_bytes(wrapped_marker_buffer->size());
  ASSERT_TRUE(wrapped_marker_buffer->GetBytes(wrapped_marker_bytes.data(),
                                              wrapped_marker_bytes.size()));

  // IsWrapped() looks at the first 3 bytes in the value's byte array.
  // Truncating the array to fewer than 3 bytes should cause IsWrapped() to
  // return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (size_t i = 0; i < 3; ++i) {
    RefPtr<IDBValue> mutant_value = IDBValue::Create(
        SharedBuffer::Create(wrapped_marker_bytes.data(), i),
        WTF::MakeUnique<Vector<RefPtr<BlobDataHandle>>>(blob_data_handles),
        WTF::MakeUnique<Vector<WebBlobInfo>>(blob_infos));

    EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(mutant_value.Get()));
  }

  // IsWrapped() looks at the first 3 bytes in the value. Flipping any bit in
  // these 3 bytes should cause IsWrapped() to return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (size_t i = 0; i < 3; ++i) {
    for (int j = 0; j < 8; ++j) {
      char mask = 1 << j;
      wrapped_marker_bytes[i] ^= mask;
      RefPtr<IDBValue> mutant_value = IDBValue::Create(
          SharedBuffer::Create(wrapped_marker_bytes.data(),
                               wrapped_marker_bytes.size()),
          WTF::MakeUnique<Vector<RefPtr<BlobDataHandle>>>(blob_data_handles),
          WTF::MakeUnique<Vector<WebBlobInfo>>(blob_infos));
      EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(mutant_value.Get()));

      wrapped_marker_bytes[i] ^= mask;
    }
  }
}

}  // namespace blink
