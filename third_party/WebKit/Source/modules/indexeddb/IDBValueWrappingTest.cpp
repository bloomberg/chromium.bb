// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBValueWrapping.h"

#include <limits>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/fileapi/Blob.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

TEST(IDBValueWrapperTest, WriteVarintOneByte) {
  Vector<char> output;

  IDBValueWrapper::WriteVarint(0, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x00', output.data()[0]);
  output.clear();

  IDBValueWrapper::WriteVarint(1, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x01', output.data()[0]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x34, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x34', output.data()[0]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x7f, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x7f', output.data()[0]);
}

TEST(IDBValueWrapperTest, WriteVarintMultiByte) {
  Vector<char> output;

  IDBValueWrapper::WriteVarint(0xff, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\x01', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x100, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x02', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x1234, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xb4', output.data()[0]);
  EXPECT_EQ('\x24', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarint(0xabcd, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xcd', output.data()[0]);
  EXPECT_EQ('\xd7', output.data()[1]);
  EXPECT_EQ('\x2', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x123456, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xd6', output.data()[0]);
  EXPECT_EQ('\xe8', output.data()[1]);
  EXPECT_EQ('\x48', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarint(0xabcdef, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\xef', output.data()[0]);
  EXPECT_EQ('\x9b', output.data()[1]);
  EXPECT_EQ('\xaf', output.data()[2]);
  EXPECT_EQ('\x05', output.data()[3]);
  output.clear();
}

TEST(IDBValueWrapperTest, WriteVarintMultiByteEdgeCases) {
  Vector<char> output;

  IDBValueWrapper::WriteVarint(0x80, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x01', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x3fff, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\x7f', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x4000, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x80', output.data()[1]);
  EXPECT_EQ('\x01', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x1fffff, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\xff', output.data()[1]);
  EXPECT_EQ('\x7f', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x200000, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x80', output.data()[1]);
  EXPECT_EQ('\x80', output.data()[2]);
  EXPECT_EQ('\x01', output.data()[3]);
  output.clear();

  IDBValueWrapper::WriteVarint(0xfffffff, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\xff', output.data()[1]);
  EXPECT_EQ('\xff', output.data()[2]);
  EXPECT_EQ('\x7f', output.data()[3]);
  output.clear();

  IDBValueWrapper::WriteVarint(0x10000000, output);
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x80', output.data()[1]);
  EXPECT_EQ('\x80', output.data()[2]);
  EXPECT_EQ('\x80', output.data()[3]);
  EXPECT_EQ('\x01', output.data()[4]);
  output.clear();

  // Maximum value of unsigned on 32-bit platforms.
  IDBValueWrapper::WriteVarint(0xffffffff, output);
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\xff', output.data()[1]);
  EXPECT_EQ('\xff', output.data()[2]);
  EXPECT_EQ('\xff', output.data()[3]);
  EXPECT_EQ('\x0f', output.data()[4]);
  output.clear();
}

// Friend class of IDBValueUnwrapper with access to its internals.
class IDBValueUnwrapperReadVarintTestHelper {
 public:
  void ReadVarint(const char* start, size_t buffer_size) {
    IDBValueUnwrapper unwrapper;

    const uint8_t* buffer_start = reinterpret_cast<const uint8_t*>(start);
    const uint8_t* buffer_end = buffer_start + buffer_size;
    unwrapper.current_ = buffer_start;
    unwrapper.end_ = buffer_end;
    success_ = unwrapper.ReadVarint(read_value_);

    ASSERT_EQ(unwrapper.end_, buffer_end)
        << "ReadVarint should not change end_";
    ASSERT_LE(unwrapper.current_, unwrapper.end_)
        << "ReadVarint should not move current_ past end_";
    consumed_bytes_ = unwrapper.current_ - buffer_start;
  }

  bool success() { return success_; }
  unsigned consumed_bytes() { return consumed_bytes_; }
  unsigned read_value() { return read_value_; }

 private:
  bool success_;
  unsigned consumed_bytes_;
  unsigned read_value_;
};

TEST(IDBValueUnwrapperTest, ReadVarintOneByte) {
  IDBValueUnwrapperReadVarintTestHelper helper;

  // Most test cases have an extra byte at the end of the input to verify that
  // the parser doesn't consume too much data.

  helper.ReadVarint("\x00\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_value());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarint("\x01\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(1U, helper.read_value());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarint("\x7f\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_value());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarint("\x7f\x01", 1);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_value());
  EXPECT_EQ(1U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarintMultiBytes) {
  IDBValueUnwrapperReadVarintTestHelper helper;

  helper.ReadVarint("\xff\x01\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xffU, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\x80\x02\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x100U, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\xb4\x24\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x1234U, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\xcd\xd7\x02\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdU, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\xd6\xe8\x48\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x123456U, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\xd6\xe8\x48\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x123456U, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\xef\x9b\xaf\x05\x01", 5);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdefU, helper.read_value());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarint("\xef\x9b\xaf\x05\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdefU, helper.read_value());
  EXPECT_EQ(4U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarintMultiByteEdgeCases) {
  IDBValueUnwrapperReadVarintTestHelper helper;

  helper.ReadVarint("\x80\x01\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x80U, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\xff\x7f\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x3fffU, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\x80\x80\x01\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x4000U, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\xff\xff\x7f\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x1fffffU, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\x80\x80\x80\x01\x01", 5);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x200000U, helper.read_value());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarint("\xff\xff\xff\x7f\x01", 5);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xfffffffU, helper.read_value());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarint("\x80\x80\x80\x80\x01\x01", 6);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x10000000U, helper.read_value());
  EXPECT_EQ(5U, helper.consumed_bytes());

  helper.ReadVarint("\xff\xff\xff\xff\x0f\x01", 6);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xffffffffU, helper.read_value());
  EXPECT_EQ(5U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarintTruncatedInput) {
  IDBValueUnwrapperReadVarintTestHelper helper;

  helper.ReadVarint("\x01", 0);
  EXPECT_FALSE(helper.success());

  helper.ReadVarint("\x80\x01", 1);
  EXPECT_FALSE(helper.success());

  helper.ReadVarint("\xff\x01", 1);
  EXPECT_FALSE(helper.success());

  helper.ReadVarint("\x80\x80\x01", 2);
  EXPECT_FALSE(helper.success());

  helper.ReadVarint("\xff\xff\x01", 2);
  EXPECT_FALSE(helper.success());

  helper.ReadVarint("\x80\x80\x80\x80\x01", 4);
  EXPECT_FALSE(helper.success());

  helper.ReadVarint("\xff\xff\xff\xff\x01", 4);
  EXPECT_FALSE(helper.success());
}

TEST(IDBValueUnwrapperTest, ReadVarintDenormalizedInput) {
  IDBValueUnwrapperReadVarintTestHelper helper;

  helper.ReadVarint("\x80\x00\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\xff\x00\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_value());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarint("\x80\x80\x00\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\x80\xff\x00\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x3f80U, helper.read_value());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarint("\x80\xff\x80\xff\x00\x01", 6);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x0fe03f80U, helper.read_value());
  EXPECT_EQ(5U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, WriteVarintMaxUnsignedRoundtrip) {
  unsigned max_value = std::numeric_limits<unsigned>::max();
  Vector<char> output;
  IDBValueWrapper::WriteVarint(max_value, output);

  IDBValueUnwrapperReadVarintTestHelper helper;
  helper.ReadVarint(output.data(), output.size());
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(max_value, helper.read_value());
  EXPECT_EQ(output.size(), helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, IsWrapped) {
  V8TestingScope scope;
  NonThrowableExceptionState non_throwable_exception_state;
  v8::Local<v8::Value> v8_true = v8::True(scope.GetIsolate());
  IDBValueWrapper wrapper(scope.GetIsolate(), v8_true,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.WrapIfBiggerThan(0);
  Vector<scoped_refptr<BlobDataHandle>> blob_data_handles;
  wrapper.ExtractBlobDataHandles(&blob_data_handles);
  Vector<WebBlobInfo>& blob_infos = wrapper.WrappedBlobInfo();
  scoped_refptr<SharedBuffer> wrapped_marker_buffer =
      wrapper.ExtractWireBytes();
  IDBKey* key = IDBKey::CreateNumber(42.0);
  IDBKeyPath key_path(String("primaryKey"));

  scoped_refptr<IDBValue> wrapped_value = IDBValue::Create(
      wrapped_marker_buffer,
      std::make_unique<Vector<scoped_refptr<BlobDataHandle>>>(
          blob_data_handles),
      std::make_unique<Vector<WebBlobInfo>>(blob_infos), key, key_path);
  EXPECT_TRUE(IDBValueUnwrapper::IsWrapped(wrapped_value.get()));

  Vector<char> wrapped_marker_bytes(wrapped_marker_buffer->size());
  ASSERT_TRUE(wrapped_marker_buffer->GetBytes(wrapped_marker_bytes.data(),
                                              wrapped_marker_bytes.size()));

  // IsWrapped() looks at the first 3 bytes in the value's byte array.
  // Truncating the array to fewer than 3 bytes should cause IsWrapped() to
  // return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (size_t i = 0; i < 3; ++i) {
    scoped_refptr<IDBValue> mutant_value = IDBValue::Create(
        SharedBuffer::Create(wrapped_marker_bytes.data(), i),
        std::make_unique<Vector<scoped_refptr<BlobDataHandle>>>(
            blob_data_handles),
        std::make_unique<Vector<WebBlobInfo>>(blob_infos), key, key_path);

    EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(mutant_value.get()));
  }

  // IsWrapped() looks at the first 3 bytes in the value. Flipping any bit in
  // these 3 bytes should cause IsWrapped() to return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (size_t i = 0; i < 3; ++i) {
    for (int j = 0; j < 8; ++j) {
      char mask = 1 << j;
      wrapped_marker_bytes[i] ^= mask;
      scoped_refptr<IDBValue> mutant_value = IDBValue::Create(
          SharedBuffer::Create(wrapped_marker_bytes.data(),
                               wrapped_marker_bytes.size()),
          std::make_unique<Vector<scoped_refptr<BlobDataHandle>>>(
              blob_data_handles),
          std::make_unique<Vector<WebBlobInfo>>(blob_infos), key, key_path);
      EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(mutant_value.get()));

      wrapped_marker_bytes[i] ^= mask;
    }
  }
}

}  // namespace blink
