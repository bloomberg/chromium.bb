// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FormDataBytesConsumer.h"

#include "core/html/forms/FormData.h"
#include "core/testing/DummyPageHolder.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using Result = BytesConsumer::Result;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using MockBytesConsumer = BytesConsumerTestUtil::MockBytesConsumer;

RefPtr<EncodedFormData> ComplexFormData() {
  RefPtr<EncodedFormData> data = EncodedFormData::Create();

  data->AppendData("foo", 3);
  data->AppendFileRange("/foo/bar/baz", 3, 4, 5);
  data->AppendFileSystemURLRange(KURL(NullURL(), "file:///foo/bar/baz"), 6, 7,
                                 8);
  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->AppendText("hello", false);
  auto size = blob_data->length();
  RefPtr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(blob_data), size);
  data->AppendBlob(blob_data_handle->Uuid(), blob_data_handle);
  Vector<char> boundary;
  boundary.Append("\0", 1);
  data->SetBoundary(boundary);
  return data;
}

class NoopClient final : public GarbageCollectedFinalized<NoopClient>,
                         public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(NoopClient);

 public:
  void OnStateChange() override {}
  String DebugName() const override { return "NoopClient"; }
};

class FormDataBytesConsumerTest : public ::testing::Test {
 public:
  FormDataBytesConsumerTest() : page_(DummyPageHolder::Create()) {}

 protected:
  Document* GetDocument() { return &page_->GetDocument(); }

  std::unique_ptr<DummyPageHolder> page_;
};

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromString) {
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer("hello, world")))
                    ->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("hello, world",
            BytesConsumerTestUtil::CharVectorToString(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromStringNonLatin) {
  constexpr UChar kCs[] = {0x3042, 0};
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer(String(kCs))))
                    ->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("\xe3\x81\x82",
            BytesConsumerTestUtil::CharVectorToString(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromArrayBuffer) {
  constexpr unsigned char kData[] = {0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3,
                                     0x42, 0x30, 0x42, 0x99, 0x88};
  DOMArrayBuffer* buffer =
      DOMArrayBuffer::Create(kData, WTF_ARRAY_LENGTH(kData));
  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer(buffer)))
                    ->Run();
  Vector<char> expected;
  expected.Append(kData, WTF_ARRAY_LENGTH(kData));

  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(expected, result.second);
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromArrayBufferView) {
  constexpr unsigned char kData[] = {0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3,
                                     0x42, 0x30, 0x42, 0x99, 0x88};
  constexpr size_t kOffset = 1, kSize = 4;
  DOMArrayBuffer* buffer =
      DOMArrayBuffer::Create(kData, WTF_ARRAY_LENGTH(kData));
  auto result =
      (new BytesConsumerTestUtil::TwoPhaseReader(new FormDataBytesConsumer(
           DOMUint8Array::Create(buffer, kOffset, kSize))))
          ->Run();
  Vector<char> expected;
  expected.Append(kData + kOffset, kSize);

  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(expected, result.second);
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromSimpleFormData) {
  RefPtr<EncodedFormData> data = EncodedFormData::Create();
  data->AppendData("foo", 3);
  data->AppendData("hoge", 4);

  auto result = (new BytesConsumerTestUtil::TwoPhaseReader(
                     new FormDataBytesConsumer(GetDocument(), data)))
                    ->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("foohoge",
            BytesConsumerTestUtil::CharVectorToString(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromComplexFormData) {
  RefPtr<EncodedFormData> data = ComplexFormData();
  MockBytesConsumer* underlying = MockBytesConsumer::Create();
  BytesConsumer* consumer =
      FormDataBytesConsumer::CreateForTesting(GetDocument(), data, underlying);
  Checkpoint checkpoint;

  const char* buffer = nullptr;
  size_t available = 0;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, BeginRead(&buffer, &available))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*underlying, EndRead(0)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  ASSERT_EQ(Result::kOk, consumer->BeginRead(&buffer, &available));
  checkpoint.Call(2);
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));
  checkpoint.Call(3);
}

TEST_F(FormDataBytesConsumerTest, EndReadCanReturnDone) {
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  const char* buffer = nullptr;
  size_t available = 0;
  ASSERT_EQ(Result::kOk, consumer->BeginRead(&buffer, &available));
  ASSERT_EQ(12u, available);
  EXPECT_EQ("hello, world", String(buffer, available));
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->EndRead(available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromString) {
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  RefPtr<BlobDataHandle> blob_data_handle = consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_EQ(String(), blob_data_handle->GetType());
  EXPECT_EQ(12u, blob_data_handle->size());
  EXPECT_FALSE(consumer->DrainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromArrayBuffer) {
  BytesConsumer* consumer =
      new FormDataBytesConsumer(DOMArrayBuffer::Create("foo", 3));
  RefPtr<BlobDataHandle> blob_data_handle = consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_EQ(String(), blob_data_handle->GetType());
  EXPECT_EQ(3u, blob_data_handle->size());
  EXPECT_FALSE(consumer->DrainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromSimpleFormData) {
  FormData* data = FormData::Create(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  RefPtr<EncodedFormData> input_form_data = data->EncodeMultiPartFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(GetDocument(), input_form_data);
  RefPtr<BlobDataHandle> blob_data_handle = consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_EQ(String(), blob_data_handle->GetType());
  EXPECT_EQ(input_form_data->FlattenToString().Utf8().length(),
            blob_data_handle->size());
  EXPECT_FALSE(consumer->DrainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromComplexFormData) {
  RefPtr<EncodedFormData> input_form_data = ComplexFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(GetDocument(), input_form_data);
  RefPtr<BlobDataHandle> blob_data_handle = consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_FALSE(consumer->DrainAsFormData());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromString) {
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  RefPtr<EncodedFormData> form_data = consumer->DrainAsFormData();
  ASSERT_TRUE(form_data);
  EXPECT_EQ("hello, world", form_data->FlattenToString());

  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromArrayBuffer) {
  BytesConsumer* consumer =
      new FormDataBytesConsumer(DOMArrayBuffer::Create("foo", 3));
  RefPtr<EncodedFormData> form_data = consumer->DrainAsFormData();
  ASSERT_TRUE(form_data);
  EXPECT_TRUE(form_data->IsSafeToSendToAnotherThread());
  EXPECT_EQ("foo", form_data->FlattenToString());

  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromSimpleFormData) {
  FormData* data = FormData::Create(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  RefPtr<EncodedFormData> input_form_data = data->EncodeMultiPartFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(GetDocument(), input_form_data);
  EXPECT_EQ(input_form_data, consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromComplexFormData) {
  RefPtr<EncodedFormData> input_form_data = ComplexFormData();

  BytesConsumer* consumer =
      new FormDataBytesConsumer(GetDocument(), input_form_data);
  EXPECT_EQ(input_form_data, consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  const char* buffer = nullptr;
  size_t available = 0;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, BeginReadAffectsDraining) {
  const char* buffer = nullptr;
  size_t available = 0;
  BytesConsumer* consumer = new FormDataBytesConsumer("hello, world");
  ASSERT_EQ(Result::kOk, consumer->BeginRead(&buffer, &available));
  EXPECT_EQ("hello, world", String(buffer, available));

  ASSERT_EQ(Result::kOk, consumer->EndRead(0));
  EXPECT_FALSE(consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, BeginReadAffectsDrainingWithComplexFormData) {
  MockBytesConsumer* underlying = MockBytesConsumer::Create();
  BytesConsumer* consumer = FormDataBytesConsumer::CreateForTesting(
      GetDocument(), ComplexFormData(), underlying);

  const char* buffer = nullptr;
  size_t available = 0;
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, BeginRead(&buffer, &available))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*underlying, EndRead(0)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(checkpoint, Call(2));
  // drainAsFormData should not be called here.
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*underlying, DrainAsBlobDataHandle(_));
  EXPECT_CALL(checkpoint, Call(4));
  // |consumer| delegates the getPublicState call to |underlying|.
  EXPECT_CALL(*underlying, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(5));

  checkpoint.Call(1);
  ASSERT_EQ(Result::kOk, consumer->BeginRead(&buffer, &available));
  ASSERT_EQ(Result::kOk, consumer->EndRead(0));
  checkpoint.Call(2);
  EXPECT_FALSE(consumer->DrainAsFormData());
  checkpoint.Call(3);
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  checkpoint.Call(4);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
  checkpoint.Call(5);
}

TEST_F(FormDataBytesConsumerTest, SetClientWithComplexFormData) {
  RefPtr<EncodedFormData> input_form_data = ComplexFormData();

  MockBytesConsumer* underlying = MockBytesConsumer::Create();
  BytesConsumer* consumer = FormDataBytesConsumer::CreateForTesting(
      GetDocument(), input_form_data, underlying);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, SetClient(_));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*underlying, ClearClient());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  consumer->SetClient(new NoopClient());
  checkpoint.Call(2);
  consumer->ClearClient();
  checkpoint.Call(3);
}

TEST_F(FormDataBytesConsumerTest, CancelWithComplexFormData) {
  RefPtr<EncodedFormData> input_form_data = ComplexFormData();

  MockBytesConsumer* underlying = MockBytesConsumer::Create();
  BytesConsumer* consumer = FormDataBytesConsumer::CreateForTesting(
      GetDocument(), input_form_data, underlying);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, Cancel());
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(1);
  consumer->Cancel();
  checkpoint.Call(2);
}

}  // namespace
}  // namespace blink
