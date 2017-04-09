// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchDataLoader.h"

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::StrictMock;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using MockFetchDataLoaderClient =
    BytesConsumerTestUtil::MockFetchDataLoaderClient;
using MockBytesConsumer = BytesConsumerTestUtil::MockBytesConsumer;
using Result = BytesConsumer::Result;

constexpr char kQuickBrownFox[] = "Quick brown fox";
constexpr size_t kQuickBrownFoxLength = 15;
constexpr size_t kQuickBrownFoxLengthWithTerminatingNull = 16;

TEST(FetchDataLoaderTest, LoadAsBlob) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();
  RefPtr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);

  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blob_data_handle->size());
  EXPECT_EQ(String("text/test"), blob_data_handle->GetType());
}

TEST(FetchDataLoaderTest, LoadAsBlobFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsBlobCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);
}

TEST(FetchDataLoaderTest,
     LoadAsBlobViaDrainAsBlobDataHandleWithSameContentType) {
  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->AppendBytes(kQuickBrownFox,
                         kQuickBrownFoxLengthWithTerminatingNull);
  blob_data->SetContentType("text/test");
  RefPtr<BlobDataHandle> input_blob_data_handle = BlobDataHandle::Create(
      std::move(blob_data), kQuickBrownFoxLengthWithTerminatingNull);

  Checkpoint checkpoint;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();
  RefPtr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(input_blob_data_handle)));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);

  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(input_blob_data_handle, blob_data_handle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blob_data_handle->size());
  EXPECT_EQ(String("text/test"), blob_data_handle->GetType());
}

TEST(FetchDataLoaderTest,
     LoadAsBlobViaDrainAsBlobDataHandleWithDifferentContentType) {
  std::unique_ptr<BlobData> blob_data = BlobData::Create();
  blob_data->AppendBytes(kQuickBrownFox,
                         kQuickBrownFoxLengthWithTerminatingNull);
  blob_data->SetContentType("text/different");
  RefPtr<BlobDataHandle> input_blob_data_handle = BlobDataHandle::Create(
      std::move(blob_data), kQuickBrownFoxLengthWithTerminatingNull);

  Checkpoint checkpoint;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsBlobHandle("text/test");
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();
  RefPtr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer,
              DrainAsBlobDataHandle(
                  BytesConsumer::BlobSizePolicy::kDisallowBlobWithInvalidSize))
      .WillOnce(Return(ByMove(input_blob_data_handle)));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);

  ASSERT_TRUE(blob_data_handle);
  EXPECT_NE(input_blob_data_handle, blob_data_handle);
  EXPECT_EQ(kQuickBrownFoxLengthWithTerminatingNull, blob_data_handle->size());
  EXPECT_EQ(String("text/test"), blob_data_handle->GetType());
}

TEST(FetchDataLoaderTest, LoadAsArrayBuffer) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsArrayBuffer();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();
  DOMArrayBuffer* array_buffer = nullptr;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadedArrayBufferMock(_))
      .WillOnce(SaveArg<0>(&array_buffer));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);

  ASSERT_TRUE(array_buffer);
  ASSERT_EQ(kQuickBrownFoxLengthWithTerminatingNull,
            array_buffer->ByteLength());
  EXPECT_STREQ(kQuickBrownFox, static_cast<const char*>(array_buffer->Data()));
}

TEST(FetchDataLoaderTest, LoadAsArrayBufferFailed) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsArrayBuffer();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLengthWithTerminatingNull),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLengthWithTerminatingNull))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsArrayBufferCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader =
      FetchDataLoader::CreateLoaderAsArrayBuffer();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);
}

TEST(FetchDataLoaderTest, LoadAsString) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLength),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLength))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client,
              DidFetchDataLoadedString(String(kQuickBrownFox)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringWithNullBytes) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  constexpr char kPattern[] = "Quick\0brown\0fox";
  constexpr size_t kLength = sizeof(kPattern);

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kPattern), SetArgPointee<1>(kLength),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(16)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kDone));
  EXPECT_CALL(*fetch_data_loader_client,
              DidFetchDataLoadedString(String(kPattern, kLength)));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringError) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(kQuickBrownFox),
                      SetArgPointee<1>(kQuickBrownFoxLength),
                      Return(Result::kOk)));
  EXPECT_CALL(*consumer, EndRead(kQuickBrownFoxLength))
      .WillOnce(Return(Result::kOk));
  EXPECT_CALL(*consumer, BeginRead(_, _)).WillOnce(Return(Result::kError));
  EXPECT_CALL(*fetch_data_loader_client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(4));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  ASSERT_TRUE(client);
  client->OnStateChange();
  checkpoint.Call(3);
  fetch_data_loader->Cancel();
  checkpoint.Call(4);
}

TEST(FetchDataLoaderTest, LoadAsStringCancel) {
  Checkpoint checkpoint;
  BytesConsumer::Client* client = nullptr;
  MockBytesConsumer* consumer = MockBytesConsumer::Create();

  FetchDataLoader* fetch_data_loader = FetchDataLoader::CreateLoaderAsString();
  MockFetchDataLoaderClient* fetch_data_loader_client =
      MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*consumer, SetClient(_)).WillOnce(SaveArg<0>(&client));
  EXPECT_CALL(*consumer, BeginRead(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(nullptr), SetArgPointee<1>(0),
                      Return(Result::kShouldWait)));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*consumer, Cancel());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  fetch_data_loader->Start(consumer, fetch_data_loader_client);
  checkpoint.Call(2);
  fetch_data_loader->Cancel();
  checkpoint.Call(3);
}

}  // namespace

}  // namespace blink
