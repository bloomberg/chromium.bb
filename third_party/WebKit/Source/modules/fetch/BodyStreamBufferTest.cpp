// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BodyStreamBuffer.h"

#include "core/html/FormData.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/network/EncodedFormData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"

namespace blink {

namespace {

using ::testing::InSequence;
using ::testing::_;
using ::testing::SaveArg;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using Command = DataConsumerHandleTestUtil::Command;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using MockFetchDataLoaderClient = DataConsumerHandleTestUtil::MockFetchDataLoaderClient;

class FakeLoaderFactory : public FetchBlobDataConsumerHandle::LoaderFactory {
public:
    PassOwnPtr<ThreadableLoader> create(ExecutionContext&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&) override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
};

class BodyStreamBufferTest : public ::testing::Test {
public:
    BodyStreamBufferTest()
    {
        m_page = DummyPageHolder::create(IntSize(1, 1));
    }
    ~BodyStreamBufferTest() override {}

protected:
    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExecutionContext* getExecutionContext() { return &m_page->document(); }

    OwnPtr<DummyPageHolder> m_page;

    ScriptValue eval(const char* s)
    {
        v8::Local<v8::String> source;
        v8::Local<v8::Script> script;
        v8::MicrotasksScope microtasks(getScriptState()->isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
        if (!v8Call(v8::String::NewFromUtf8(getScriptState()->isolate(), s, v8::NewStringType::kNormal), source)) {
            ADD_FAILURE();
            return ScriptValue();
        }
        if (!v8Call(v8::Script::Compile(getScriptState()->context(), source), script)) {
            ADD_FAILURE() << "Compilation fails";
            return ScriptValue();
        }
        return ScriptValue(getScriptState(), script->Run(getScriptState()->context()));
    }
    ScriptValue evalWithPrintingError(const char* s)
    {
        v8::TryCatch block(getScriptState()->isolate());
        ScriptValue r = eval(s);
        if (block.HasCaught()) {
            ADD_FAILURE() << toCoreString(block.Exception()->ToString(getScriptState()->isolate())).utf8().data();
            block.ReThrow();
        }
        return r;
    }
};

TEST_F(BodyStreamBufferTest, Tee)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client1 = MockFetchDataLoaderClient::create();
    MockFetchDataLoaderClient* client2 = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client1, didFetchDataLoadedString(String("hello, world")));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client2, didFetchDataLoadedString(String("hello, world")));
    EXPECT_CALL(checkpoint, Call(4));

    OwnPtr<DataConsumerHandleTestUtil::ReplayingHandle> handle = DataConsumerHandleTestUtil::ReplayingHandle::create();
    handle->add(DataConsumerHandleTestUtil::Command(DataConsumerHandleTestUtil::Command::Data, "hello, "));
    handle->add(DataConsumerHandleTestUtil::Command(DataConsumerHandleTestUtil::Command::Data, "world"));
    handle->add(DataConsumerHandleTestUtil::Command(DataConsumerHandleTestUtil::Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));

    BodyStreamBuffer* new1;
    BodyStreamBuffer* new2;
    buffer->tee(&new1, &new2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    new1->startLoading(FetchDataLoader::createLoaderAsString(), client1);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    new2->startLoading(FetchDataLoader::createLoaderAsString(), client2);
    checkpoint.Call(3);
    testing::runPendingTasks();
    checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, TeeFromHandleMadeFromStream)
{
    ScriptState::Scope scope(getScriptState());
    ScriptValue stream = evalWithPrintingError(
        "stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue(new Uint8Array([0x41, 0x42]));"
        "controller.enqueue(new Uint8Array([0x55, 0x58]));"
        "controller.close();"
        "stream");
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client1 = MockFetchDataLoaderClient::create();
    MockFetchDataLoaderClient* client2 = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client1, didFetchDataLoadedString(String("ABUX")));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client2, didFetchDataLoadedString(String("ABUX")));
    EXPECT_CALL(checkpoint, Call(4));

    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), stream);

    BodyStreamBuffer* new1;
    BodyStreamBuffer* new2;
    buffer->tee(&new1, &new2);

    EXPECT_TRUE(buffer->isStreamLocked());
    // Note that this behavior is slightly different from for the behavior of
    // a BodyStreamBuffer made from a FetchDataConsumerHandle. See the above
    // test. In this test, the stream will get disturbed when the microtask
    // is performed.
    // TODO(yhirano): A uniformed behavior is preferred.
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    v8::MicrotasksScope::PerformCheckpoint(getScriptState()->isolate());

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    new1->startLoading(FetchDataLoader::createLoaderAsString(), client1);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    new2->startLoading(FetchDataLoader::createLoaderAsString(), client2);
    checkpoint.Call(3);
    testing::runPendingTasks();
    checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandle)
{
    OwnPtr<BlobData> data = BlobData::create();
    data->appendText("hello", false);
    auto size = data->length();
    RefPtr<BlobDataHandle> blobDataHandle = BlobDataHandle::create(std::move(data), size);
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), FetchBlobDataConsumerHandle::create(getExecutionContext(), blobDataHandle, new FakeLoaderFactory));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    RefPtr<BlobDataHandle> outputBlobDataHandle = buffer->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_EQ(blobDataHandle, outputBlobDataHandle);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandleReturnsNull)
{
    // This handle is not drainable.
    OwnPtr<FetchDataConsumerHandle> handle = createFetchDataConsumerHandleFromWebHandle(createWaitingDataConsumerHandle());
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), std::move(handle));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    EXPECT_FALSE(buffer->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, DrainAsBlobFromBufferMadeFromBufferMadeFromStream)
{
    ScriptState::Scope scope(getScriptState());
    ScriptValue stream = evalWithPrintingError("new ReadableStream()");
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), stream);

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());

    EXPECT_FALSE(buffer->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize));

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());
}

TEST_F(BodyStreamBufferTest, DrainAsFormData)
{
    FormData* data = FormData::create(UTF8Encoding());
    data->append("name1", "value1");
    data->append("name2", "value2");
    RefPtr<EncodedFormData> inputFormData = data->encodeMultiPartFormData();

    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), FetchFormDataConsumerHandle::create(getExecutionContext(), inputFormData));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    RefPtr<EncodedFormData> outputFormData = buffer->drainAsFormData();

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_EQ(outputFormData->flattenToString(), inputFormData->flattenToString());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataReturnsNull)
{
    // This handle is not drainable.
    OwnPtr<FetchDataConsumerHandle> handle = createFetchDataConsumerHandleFromWebHandle(createWaitingDataConsumerHandle());
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), std::move(handle));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    EXPECT_FALSE(buffer->drainAsFormData());

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataFromBufferMadeFromBufferMadeFromStream)
{
    ScriptState::Scope scope(getScriptState());
    ScriptValue stream = evalWithPrintingError("new ReadableStream()");
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), stream);

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());

    EXPECT_FALSE(buffer->drainAsFormData());

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsArrayBuffer)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();
    DOMArrayBuffer* arrayBuffer = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedArrayBufferMock(_)).WillOnce(SaveArg<0>(&arrayBuffer));
    EXPECT_CALL(checkpoint, Call(2));

    OwnPtr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsArrayBuffer(), client);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    ASSERT_TRUE(arrayBuffer);
    EXPECT_EQ("hello", String(static_cast<const char*>(arrayBuffer->data()), arrayBuffer->byteLength()));
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsBlob)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();
    RefPtr<BlobDataHandle> blobDataHandle;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedBlobHandleMock(_)).WillOnce(SaveArg<0>(&blobDataHandle));
    EXPECT_CALL(checkpoint, Call(2));

    OwnPtr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsBlobHandle("text/plain"), client);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_EQ(5u, blobDataHandle->size());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsString)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedString(String("hello")));
    EXPECT_CALL(checkpoint, Call(2));

    OwnPtr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadClosedHandle)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedString(String("")));
    EXPECT_CALL(checkpoint, Call(2));

    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle()));

    EXPECT_TRUE(buffer->isStreamReadable());
    testing::runPendingTasks();
    EXPECT_TRUE(buffer->isStreamClosed());

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);
    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadErroredHandle)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadFailed());
    EXPECT_CALL(checkpoint, Call(2));

    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(createUnexpectedErrorDataConsumerHandle()));

    EXPECT_TRUE(buffer->isStreamReadable());
    testing::runPendingTasks();
    EXPECT_TRUE(buffer->isStreamErrored());

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);
    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoaderShouldBeKeptAliveByBodyStreamBuffer)
{
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedString(String("hello")));
    EXPECT_CALL(checkpoint, Call(2));

    OwnPtr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    Persistent<BodyStreamBuffer> buffer = new BodyStreamBuffer(getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);

    ThreadHeap::collectAllGarbage();
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
}

// TODO(hiroshige): Merge this class into MockFetchDataConsumerHandle.
class MockFetchDataConsumerHandleWithMockDestructor : public DataConsumerHandleTestUtil::MockFetchDataConsumerHandle {
public:
    static PassOwnPtr<::testing::StrictMock<MockFetchDataConsumerHandleWithMockDestructor>> create() { return adoptPtr(new ::testing::StrictMock<MockFetchDataConsumerHandleWithMockDestructor>); }

    ~MockFetchDataConsumerHandleWithMockDestructor() override
    {
        destruct();
    }

    MOCK_METHOD0(destruct, void());
};

TEST_F(BodyStreamBufferTest, SourceHandleAndReaderShouldBeDestructedWhenCanceled)
{
    ScriptState::Scope scope(getScriptState());
    using MockHandle = MockFetchDataConsumerHandleWithMockDestructor;
    using MockReader = DataConsumerHandleTestUtil::MockFetchDataConsumerReader;
    OwnPtr<MockHandle> handle = MockHandle::create();
    OwnPtr<MockReader> reader = MockReader::create();

    Checkpoint checkpoint;
    InSequence s;

    EXPECT_CALL(*handle, obtainReaderInternal(_)).WillOnce(::testing::Return(reader.get()));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*handle, destruct());
    EXPECT_CALL(checkpoint, Call(2));

    // |reader| is adopted by |obtainReader|.
    ASSERT_TRUE(reader.leakPtr());

    BodyStreamBuffer* buffer = new BodyStreamBuffer(getScriptState(), std::move(handle));
    checkpoint.Call(1);
    ScriptValue reason(getScriptState(), v8String(getScriptState()->isolate(), "reason"));
    buffer->cancelSource(getScriptState(), reason);
    checkpoint.Call(2);
}

} // namespace

} // namespace blink
