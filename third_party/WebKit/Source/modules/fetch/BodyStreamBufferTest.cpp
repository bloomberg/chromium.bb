// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BodyStreamBuffer.h"

#include <memory>
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/html/forms/FormData.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "modules/fetch/BytesConsumerTestUtil.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/network/EncodedFormData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;
using ::testing::SaveArg;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using BytesConsumerCommand = BytesConsumerTestUtil::Command;
using ReplayingBytesConsumer = BytesConsumerTestUtil::ReplayingBytesConsumer;
using MockFetchDataLoaderClient =
    BytesConsumerTestUtil::MockFetchDataLoaderClient;

class BodyStreamBufferTest : public ::testing::Test {
 protected:
  ScriptValue Eval(ScriptState* script_state, const char* s) {
    v8::Local<v8::String> source;
    v8::Local<v8::Script> script;
    v8::MicrotasksScope microtasks(script_state->GetIsolate(),
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (!v8::String::NewFromUtf8(script_state->GetIsolate(), s,
                                 v8::NewStringType::kNormal)
             .ToLocal(&source)) {
      ADD_FAILURE();
      return ScriptValue();
    }
    if (!v8::Script::Compile(script_state->GetContext(), source)
             .ToLocal(&script)) {
      ADD_FAILURE() << "Compilation fails";
      return ScriptValue();
    }
    return ScriptValue(script_state, script->Run(script_state->GetContext()));
  }
  ScriptValue EvalWithPrintingError(ScriptState* script_state, const char* s) {
    v8::TryCatch block(script_state->GetIsolate());
    ScriptValue r = Eval(script_state, s);
    if (block.HasCaught()) {
      ADD_FAILURE() << ToCoreString(block.Exception()->ToString(
                                        script_state->GetIsolate()))
                           .Utf8()
                           .data();
      block.ReThrow();
    }
    return r;
  }
};

TEST_F(BodyStreamBufferTest, Tee) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client1 = MockFetchDataLoaderClient::Create();
  MockFetchDataLoaderClient* client2 = MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(*client1, DidFetchDataLoadedString(String("hello, world")));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*client2, DidFetchDataLoadedString(String("hello, world")));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(checkpoint, Call(4));

  ReplayingBytesConsumer* src =
      new ReplayingBytesConsumer(&scope.GetDocument());
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "hello, "));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "world"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kDone));
  BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.GetScriptState(), src);

  BodyStreamBuffer* new1;
  BodyStreamBuffer* new2;
  buffer->Tee(&new1, &new2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  checkpoint.Call(0);
  new1->StartLoading(FetchDataLoader::CreateLoaderAsString(), client1);
  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);

  new2->StartLoading(FetchDataLoader::CreateLoaderAsString(), client2);
  checkpoint.Call(3);
  testing::RunPendingTasks();
  checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, TeeFromHandleMadeFromStream) {
  V8TestingScope scope;
  ScriptValue stream = EvalWithPrintingError(
      scope.GetScriptState(),
      "stream = new ReadableStream({start: c => controller = c});"
      "controller.enqueue(new Uint8Array([0x41, 0x42]));"
      "controller.enqueue(new Uint8Array([0x55, 0x58]));"
      "controller.close();"
      "stream");
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client1 = MockFetchDataLoaderClient::Create();
  MockFetchDataLoaderClient* client2 = MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client1, DidFetchDataLoadedString(String("ABUX")));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client2, DidFetchDataLoadedString(String("ABUX")));
  EXPECT_CALL(checkpoint, Call(4));

  BodyStreamBuffer* buffer =
      new BodyStreamBuffer(scope.GetScriptState(), stream);

  BodyStreamBuffer* new1;
  BodyStreamBuffer* new2;
  buffer->Tee(&new1, &new2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  // Note that this behavior is slightly different from for the behavior of
  // a BodyStreamBuffer made from a BytesConsumer. See the above test. In this
  // test, the stream will get disturbed when the microtask is performed.
  // TODO(yhirano): A uniformed behavior is preferred.
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  new1->StartLoading(FetchDataLoader::CreateLoaderAsString(), client1);
  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);

  new2->StartLoading(FetchDataLoader::CreateLoaderAsString(), client2);
  checkpoint.Call(3);
  testing::RunPendingTasks();
  checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandle) {
  V8TestingScope scope;
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendText("hello", false);
  auto size = data->length();
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(data), size);
  BodyStreamBuffer* buffer = new BodyStreamBuffer(
      scope.GetScriptState(),
      new BlobBytesConsumer(scope.GetExecutionContext(), blob_data_handle));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
  scoped_refptr<BlobDataHandle> output_blob_data_handle =
      buffer->DrainAsBlobDataHandle(
          BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_EQ(blob_data_handle, output_blob_data_handle);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandleReturnsNull) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = new ReplayingBytesConsumer(&scope.GetDocument());
  BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.GetScriptState(), src);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  EXPECT_FALSE(buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest,
       DrainAsBlobFromBufferMadeFromBufferMadeFromStream) {
  V8TestingScope scope;
  ScriptValue stream =
      EvalWithPrintingError(scope.GetScriptState(), "new ReadableStream()");
  BodyStreamBuffer* buffer =
      new BodyStreamBuffer(scope.GetScriptState(), stream);

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());

  EXPECT_FALSE(buffer->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize));

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());
}

TEST_F(BodyStreamBufferTest, DrainAsFormData) {
  V8TestingScope scope;
  FormData* data = FormData::Create(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  scoped_refptr<EncodedFormData> input_form_data =
      data->EncodeMultiPartFormData();

  BodyStreamBuffer* buffer = new BodyStreamBuffer(
      scope.GetScriptState(),
      new FormDataBytesConsumer(scope.GetExecutionContext(), input_form_data));

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
  scoped_refptr<EncodedFormData> output_form_data = buffer->DrainAsFormData();

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_EQ(output_form_data->FlattenToString(),
            input_form_data->FlattenToString());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataReturnsNull) {
  V8TestingScope scope;
  // This BytesConsumer is not drainable.
  BytesConsumer* src = new ReplayingBytesConsumer(&scope.GetDocument());
  BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.GetScriptState(), src);

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  EXPECT_FALSE(buffer->DrainAsFormData());

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest,
       DrainAsFormDataFromBufferMadeFromBufferMadeFromStream) {
  V8TestingScope scope;
  ScriptValue stream =
      EvalWithPrintingError(scope.GetScriptState(), "new ReadableStream()");
  BodyStreamBuffer* buffer =
      new BodyStreamBuffer(scope.GetScriptState(), stream);

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());

  EXPECT_FALSE(buffer->DrainAsFormData());

  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->IsStreamReadable());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsArrayBuffer) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::Create();
  DOMArrayBuffer* array_buffer = nullptr;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedArrayBufferMock(_))
      .WillOnce(SaveArg<0>(&array_buffer));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src =
      new ReplayingBytesConsumer(&scope.GetDocument());
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kWait));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "hello"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kDone));
  BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.GetScriptState(), src);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsArrayBuffer(), client);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
  ASSERT_TRUE(array_buffer);
  EXPECT_EQ("hello", String(static_cast<const char*>(array_buffer->Data()),
                            array_buffer->ByteLength()));
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsBlob) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::Create();
  scoped_refptr<BlobDataHandle> blob_data_handle;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedBlobHandleMock(_))
      .WillOnce(SaveArg<0>(&blob_data_handle));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src =
      new ReplayingBytesConsumer(&scope.GetDocument());
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kWait));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "hello"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kDone));
  BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.GetScriptState(), src);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsBlobHandle("text/plain"),
                       client);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
  EXPECT_EQ(5u, blob_data_handle->size());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsString) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("hello")));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src =
      new ReplayingBytesConsumer(&scope.GetDocument());
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kWait));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "hello"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kDone));
  BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.GetScriptState(), src);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(), client);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_TRUE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadClosedHandle) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("")));
  EXPECT_CALL(checkpoint, Call(2));

  BodyStreamBuffer* buffer = new BodyStreamBuffer(
      scope.GetScriptState(), BytesConsumer::CreateClosed());

  EXPECT_TRUE(buffer->IsStreamClosed());

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(), client);
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadErroredHandle) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadFailed());
  EXPECT_CALL(checkpoint, Call(2));

  BodyStreamBuffer* buffer = new BodyStreamBuffer(
      scope.GetScriptState(),
      BytesConsumer::CreateErrored(BytesConsumer::Error()));

  EXPECT_TRUE(buffer->IsStreamErrored());

  EXPECT_FALSE(buffer->IsStreamLocked());
  EXPECT_FALSE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());

  checkpoint.Call(1);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(), client);
  checkpoint.Call(2);

  EXPECT_TRUE(buffer->IsStreamLocked());
  EXPECT_TRUE(buffer->IsStreamDisturbed());
  EXPECT_FALSE(buffer->HasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoaderShouldBeKeptAliveByBodyStreamBuffer) {
  V8TestingScope scope;
  Checkpoint checkpoint;
  MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::Create();

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, DidFetchDataLoadedString(String("hello")));
  EXPECT_CALL(checkpoint, Call(2));

  ReplayingBytesConsumer* src =
      new ReplayingBytesConsumer(&scope.GetDocument());
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kWait));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "hello"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kDone));
  Persistent<BodyStreamBuffer> buffer =
      new BodyStreamBuffer(scope.GetScriptState(), src);
  buffer->StartLoading(FetchDataLoader::CreateLoaderAsString(), client);

  ThreadState::Current()->CollectAllGarbage();
  checkpoint.Call(1);
  testing::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(BodyStreamBufferTest, SourceShouldBeCanceledWhenCanceled) {
  V8TestingScope scope;
  ReplayingBytesConsumer* consumer =
      new BytesConsumerTestUtil::ReplayingBytesConsumer(
          scope.GetExecutionContext());

  BodyStreamBuffer* buffer =
      new BodyStreamBuffer(scope.GetScriptState(), consumer);
  ScriptValue reason(scope.GetScriptState(),
                     V8String(scope.GetScriptState()->GetIsolate(), "reason"));
  EXPECT_FALSE(consumer->IsCancelled());
  buffer->Cancel(scope.GetScriptState(), reason);
  EXPECT_TRUE(consumer->IsCancelled());
}

TEST_F(BodyStreamBufferTest, NestedPull) {
  V8TestingScope scope;
  ReplayingBytesConsumer* src =
      new ReplayingBytesConsumer(&scope.GetDocument());
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kWait));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kData, "hello"));
  src->Add(BytesConsumerCommand(BytesConsumerCommand::kError));
  Persistent<BodyStreamBuffer> buffer =
      new BodyStreamBuffer(scope.GetScriptState(), src);

  auto result =
      scope.GetScriptState()->GetContext()->Global()->CreateDataProperty(
          scope.GetScriptState()->GetContext(),
          V8String(scope.GetIsolate(), "stream"), buffer->Stream().V8Value());

  ASSERT_TRUE(result.IsJust());
  ASSERT_TRUE(result.FromJust());

  ScriptValue stream = EvalWithPrintingError(scope.GetScriptState(),
                                             "reader = stream.getReader();");
  ASSERT_FALSE(stream.IsEmpty());

  EvalWithPrintingError(scope.GetScriptState(), "reader.read();");
  EvalWithPrintingError(scope.GetScriptState(), "reader.read();");

  testing::RunPendingTasks();
  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
}

}  // namespace

}  // namespace blink
