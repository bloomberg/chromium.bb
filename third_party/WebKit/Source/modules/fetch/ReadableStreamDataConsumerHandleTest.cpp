// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/ReadableStreamDataConsumerHandle.h"

#include "bindings/core/v8/ReadableStreamOperations.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/heap/Handle.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

// TODO(yhirano): Add cross-thread tests once the handle gets thread-safe.
namespace blink {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using Result = WebDataConsumerHandle::Result;
const Result kOk = WebDataConsumerHandle::Ok;
const Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;
const Result kDone = WebDataConsumerHandle::Done;
using Flags = WebDataConsumerHandle::Flags;
const Flags kNone = WebDataConsumerHandle::FlagNone;

class MockClient : public GarbageCollectedFinalized<MockClient>, public WebDataConsumerHandle::Client {
public:
    static StrictMock<MockClient>* create() { return new StrictMock<MockClient>(); }
    MOCK_METHOD0(didGetReadable, void());

    DEFINE_INLINE_TRACE() {}

protected:
    MockClient() = default;
};

class ReadableStreamDataConsumerHandleTest : public ::testing::Test {
public:
    ReadableStreamDataConsumerHandleTest()
        : m_page(DummyPageHolder::create())
    {
    }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return scriptState()->isolate(); }

    v8::MaybeLocal<v8::Value> eval(const char* s)
    {
        v8::Local<v8::String> source;
        v8::Local<v8::Script> script;
        if (!v8Call(v8::String::NewFromUtf8(isolate(), s, v8::NewStringType::kNormal), source)) {
            ADD_FAILURE();
            return v8::MaybeLocal<v8::Value>();
        }
        if (!v8Call(v8::Script::Compile(scriptState()->context(), source), script)) {
            ADD_FAILURE() << "Compilation fails";
            return v8::MaybeLocal<v8::Value>();
        }
        return script->Run(scriptState()->context());
    }
    v8::MaybeLocal<v8::Value> evalWithPrintingError(const char* s)
    {
        v8::TryCatch block(isolate());
        v8::MaybeLocal<v8::Value> r = eval(s);
        if (block.HasCaught()) {
            ADD_FAILURE() << toCoreString(block.Exception()->ToString(isolate())).utf8().data();
            block.ReThrow();
        }
        return r;
    }

    PassOwnPtr<ReadableStreamDataConsumerHandle> createHandle(ScriptValue stream)
    {
        NonThrowableExceptionState es;
        ScriptValue reader = ReadableStreamOperations::getReader(scriptState(), stream, es);
        ASSERT(!reader.isEmpty());
        ASSERT(reader.v8Value()->IsObject());
        return ReadableStreamDataConsumerHandle::create(scriptState(), reader);
    }

    void gc() { V8GCController::collectAllGarbageForTesting(isolate()); }

private:
    OwnPtr<DummyPageHolder> m_page;
};

TEST_F(ReadableStreamDataConsumerHandleTest, Create)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError("new ReadableStream"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));

    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
}

TEST_F(ReadableStreamDataConsumerHandleTest, EmptyStream)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "new ReadableStream({start: c => c.close()})"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));

    char c;
    size_t readBytes;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->read(&c, 1, kNone, &readBytes));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kDone, reader->read(&c, 1, kNone, &readBytes));
}

TEST_F(ReadableStreamDataConsumerHandleTest, ErroredStream)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "new ReadableStream({start: c => c.error()})"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));

    char c;
    size_t readBytes;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->read(&c, 1, kNone, &readBytes));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kUnexpectedError, reader->read(&c, 1, kNone, &readBytes));
}

TEST_F(ReadableStreamDataConsumerHandleTest, Read)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "var controller;"
        "var stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue(new Uint8Array([0x43, 0x44, 0x45, 0x46]));"
        "controller.enqueue(new Uint8Array([0x47, 0x48, 0x49, 0x4a]));"
        "controller.close();"
        "stream"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(5));

    char buffer[3];
    size_t readBytes;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->read(buffer, 3, kNone, &readBytes));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kOk, reader->read(buffer, 3, kNone, &readBytes));
    EXPECT_EQ(3u, readBytes);
    EXPECT_EQ(0x43, buffer[0]);
    EXPECT_EQ(0x44, buffer[1]);
    EXPECT_EQ(0x45, buffer[2]);
    EXPECT_EQ(kOk, reader->read(buffer, 3, kNone, &readBytes));
    EXPECT_EQ(1u, readBytes);
    EXPECT_EQ(0x46, buffer[0]);
    EXPECT_EQ(kShouldWait, reader->read(buffer, 3, kNone, &readBytes));
    testing::runPendingTasks();
    checkpoint.Call(4);
    EXPECT_EQ(kOk, reader->read(buffer, 3, kNone, &readBytes));
    EXPECT_EQ(3u, readBytes);
    EXPECT_EQ(0x47, buffer[0]);
    EXPECT_EQ(0x48, buffer[1]);
    EXPECT_EQ(0x49, buffer[2]);
    EXPECT_EQ(kOk, reader->read(buffer, 3, kNone, &readBytes));
    EXPECT_EQ(1u, readBytes);
    EXPECT_EQ(0x4a, buffer[0]);
    EXPECT_EQ(kShouldWait, reader->read(buffer, 3, kNone, &readBytes));
    testing::runPendingTasks();
    checkpoint.Call(5);
    EXPECT_EQ(kDone, reader->read(buffer, 3, kNone, &readBytes));
}

TEST_F(ReadableStreamDataConsumerHandleTest, TwoPhaseRead)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "var controller;"
        "var stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue(new Uint8Array([0x43, 0x44, 0x45, 0x46]));"
        "controller.enqueue(new Uint8Array([0x47, 0x48, 0x49, 0x4a]));"
        "controller.close();"
        "stream"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(5));

    const void* buffer;
    size_t available;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kOk, reader->beginRead(&buffer, kNone, &available));
    EXPECT_EQ(4u, available);
    EXPECT_EQ(0x43, static_cast<const char*>(buffer)[0]);
    EXPECT_EQ(0x44, static_cast<const char*>(buffer)[1]);
    EXPECT_EQ(0x45, static_cast<const char*>(buffer)[2]);
    EXPECT_EQ(0x46, static_cast<const char*>(buffer)[3]);
    EXPECT_EQ(kOk, reader->endRead(0));
    EXPECT_EQ(kOk, reader->beginRead(&buffer, kNone, &available));
    EXPECT_EQ(4u, available);
    EXPECT_EQ(0x43, static_cast<const char*>(buffer)[0]);
    EXPECT_EQ(0x44, static_cast<const char*>(buffer)[1]);
    EXPECT_EQ(0x45, static_cast<const char*>(buffer)[2]);
    EXPECT_EQ(0x46, static_cast<const char*>(buffer)[3]);
    EXPECT_EQ(kOk, reader->endRead(1));
    EXPECT_EQ(kOk, reader->beginRead(&buffer, kNone, &available));
    EXPECT_EQ(3u, available);
    EXPECT_EQ(0x44, static_cast<const char*>(buffer)[0]);
    EXPECT_EQ(0x45, static_cast<const char*>(buffer)[1]);
    EXPECT_EQ(0x46, static_cast<const char*>(buffer)[2]);
    EXPECT_EQ(kOk, reader->endRead(3));
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    checkpoint.Call(4);
    EXPECT_EQ(kOk, reader->beginRead(&buffer, kNone, &available));
    EXPECT_EQ(4u, available);
    EXPECT_EQ(0x47, static_cast<const char*>(buffer)[0]);
    EXPECT_EQ(0x48, static_cast<const char*>(buffer)[1]);
    EXPECT_EQ(0x49, static_cast<const char*>(buffer)[2]);
    EXPECT_EQ(0x4a, static_cast<const char*>(buffer)[3]);
    EXPECT_EQ(kOk, reader->endRead(4));
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    checkpoint.Call(5);
    EXPECT_EQ(kDone, reader->beginRead(&buffer, kNone, &available));
}

TEST_F(ReadableStreamDataConsumerHandleTest, EnqueueUndefined)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "var controller;"
        "var stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue(undefined);"
        "controller.close();"
        "stream"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));

    const void* buffer;
    size_t available;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kUnexpectedError, reader->beginRead(&buffer, kNone, &available));
}

TEST_F(ReadableStreamDataConsumerHandleTest, EnqueueNull)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "var controller;"
        "var stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue(null);"
        "controller.close();"
        "stream"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));

    const void* buffer;
    size_t available;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kUnexpectedError, reader->beginRead(&buffer, kNone, &available));
}

TEST_F(ReadableStreamDataConsumerHandleTest, EnqueueString)
{
    ScriptState::Scope scope(scriptState());
    ScriptValue stream(scriptState(), evalWithPrintingError(
        "var controller;"
        "var stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue('hello');"
        "controller.close();"
        "stream"));
    ASSERT_FALSE(stream.isEmpty());
    OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
    ASSERT_TRUE(handle);
    MockClient* client = MockClient::create();
    Checkpoint checkpoint;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(3));

    const void* buffer;
    size_t available;
    OwnPtr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(client);
    ASSERT_TRUE(reader);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    checkpoint.Call(3);
    EXPECT_EQ(kUnexpectedError, reader->beginRead(&buffer, kNone, &available));
}

TEST_F(ReadableStreamDataConsumerHandleTest, StreamReaderShouldBeWeak)
{
    OwnPtr<FetchDataConsumerHandle::Reader> reader;
    Checkpoint checkpoint;
    Persistent<MockClient> client = MockClient::create();
    ScriptValue stream;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(4));

    {
        // We need this scope to collect local handles.
        ScriptState::Scope scope(scriptState());
        stream = ScriptValue(scriptState(), evalWithPrintingError("new ReadableStream()"));
        ASSERT_FALSE(stream.isEmpty());
        OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
        ASSERT_TRUE(handle);

        reader = handle->obtainReader(client);
        ASSERT_TRUE(reader);
    }

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    stream.clear();
    gc();
    checkpoint.Call(3);
    testing::runPendingTasks();

    checkpoint.Call(4);
    const void* buffer;
    size_t available;
    EXPECT_EQ(kUnexpectedError, reader->beginRead(&buffer, kNone, &available));
}

TEST_F(ReadableStreamDataConsumerHandleTest, StreamReaderShouldBeWeakWhenReading)
{
    OwnPtr<FetchDataConsumerHandle::Reader> reader;
    Checkpoint checkpoint;
    Persistent<MockClient> client = MockClient::create();
    ScriptValue stream;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*client, didGetReadable());
    EXPECT_CALL(checkpoint, Call(5));

    {
        // We need this scope to collect local handles.
        ScriptState::Scope scope(scriptState());
        stream = ScriptValue(scriptState(), evalWithPrintingError("new ReadableStream()"));
        ASSERT_FALSE(stream.isEmpty());
        OwnPtr<ReadableStreamDataConsumerHandle> handle = createHandle(stream);
        ASSERT_TRUE(handle);

        reader = handle->obtainReader(client);
        ASSERT_TRUE(reader);
    }

    const void* buffer;
    size_t available;
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));

    testing::runPendingTasks();
    checkpoint.Call(3);
    stream.clear();
    gc();
    checkpoint.Call(4);
    testing::runPendingTasks();

    checkpoint.Call(5);
    EXPECT_EQ(kUnexpectedError, reader->beginRead(&buffer, kNone, &available));
}

} // namespace

} // namespace blink

