// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_writable_stream_wrapper.h"

#include "base/test/mock_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::StrictMock;

// The purpose of this class is to ensure that the data pipe is reset before the
// V8TestingScope is destroyed, so that the TCPWritableStreamWrapper object
// doesn't try to create a DOMException after the ScriptState has gone away.
class StreamCreator {
  STACK_ALLOCATED();

 public:
  StreamCreator() = default;
  ~StreamCreator() {
    ClosePipe();

    // Let the TCPWritableStreamWrapper object respond to the closure if it
    // needs to.
    test::RunPendingTasks();
  }

  // The default value of |capacity| means some sensible value selected by mojo.
  TCPWritableStreamWrapper* Create(const V8TestingScope& scope,
                                   uint32_t capacity = 0) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = capacity;

    mojo::ScopedDataPipeProducerHandle data_pipe_producer;
    MojoResult result =
        mojo::CreateDataPipe(&options, data_pipe_producer, data_pipe_consumer_);
    if (result != MOJO_RESULT_OK) {
      ADD_FAILURE() << "CreateDataPipe() returned " << result;
    }

    auto* script_state = scope.GetScriptState();
    auto* tcp_writable_stream_wrapper =
        MakeGarbageCollected<TCPWritableStreamWrapper>(
            script_state,
            base::BindOnce(&StreamCreator::OnAbort, base::Unretained(this)),
            std::move(data_pipe_producer));
    return tcp_writable_stream_wrapper;
  }

  void ClosePipe() { data_pipe_consumer_.reset(); }

  // Reads everything from |data_pipe_consumer_| and returns it in a vector.
  Vector<uint8_t> ReadAllPendingData() {
    Vector<uint8_t> data;
    const void* buffer = nullptr;
    uint32_t buffer_num_bytes = 0;
    MojoResult result = data_pipe_consumer_->BeginReadData(
        &buffer, &buffer_num_bytes, MOJO_BEGIN_READ_DATA_FLAG_NONE);

    switch (result) {
      case MOJO_RESULT_OK:
        break;

      case MOJO_RESULT_SHOULD_WAIT:  // No more data yet.
        return data;

      default:
        ADD_FAILURE() << "BeginReadData() failed: " << result;
        return data;
    }

    data.Append(static_cast<const uint8_t*>(buffer), buffer_num_bytes);
    data_pipe_consumer_->EndReadData(buffer_num_bytes);
    return data;
  }

  void OnAbort() { on_abort_called_ = true; }
  bool HasAborted() const { return on_abort_called_; }

 private:
  bool on_abort_called_ = false;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer_;
};

TEST(TCPWritableStreamWrapperTest, Create) {
  V8TestingScope scope;
  StreamCreator stream_creator;
  auto* tcp_writable_stream_wrapper = stream_creator.Create(scope);
  EXPECT_TRUE(tcp_writable_stream_wrapper->Writable());
}

TEST(TCPWritableStreamWrapperTest, WriteArrayBuffer) {
  V8TestingScope scope;
  StreamCreator stream_creator;
  auto* tcp_writable_stream_wrapper = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create("A", 1);
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre('A'));
}

TEST(TCPWritableStreamWrapperTest, WriteArrayBufferView) {
  V8TestingScope scope;
  StreamCreator stream_creator;
  auto* tcp_writable_stream_wrapper = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* buffer = DOMArrayBuffer::Create("*B", 2);
  // Create a view into the buffer with offset 1, ie. "B".
  auto* chunk = DOMUint8Array::Create(buffer, 1, 1);
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre('B'));
}

bool IsAllNulls(base::span<const uint8_t> data) {
  return std::all_of(data.begin(), data.end(), [](uint8_t c) { return !c; });
}

TEST(TCPWritableStreamWrapperTest, AsyncWrite) {
  V8TestingScope scope;
  StreamCreator stream_creator;
  // Set a large pipe capacity, so any platform-specific excess is dwarfed in
  // size.
  constexpr uint32_t kPipeCapacity = 512u * 1024u;
  auto* tcp_writable_stream_wrapper =
      stream_creator.Create(scope, kPipeCapacity);

  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  // Write a chunk that definitely will not fit in the pipe.
  const size_t kChunkSize = kPipeCapacity * 3;
  auto* chunk = DOMArrayBuffer::Create(kChunkSize, 1);
  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), result);

  // Let the first pipe write complete.
  test::RunPendingTasks();

  // Let microtasks run just in case write() returns prematurely.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  ASSERT_FALSE(tester.IsFulfilled());

  // Read the first part of the data.
  auto data1 = stream_creator.ReadAllPendingData();
  EXPECT_LT(data1.size(), kChunkSize);

  // Verify the data wasn't corrupted.
  EXPECT_TRUE(IsAllNulls(data1));

  // Allow the asynchronous pipe write to happen.
  test::RunPendingTasks();

  // Read the second part of the data.
  auto data2 = stream_creator.ReadAllPendingData();
  EXPECT_TRUE(IsAllNulls(data2));

  test::RunPendingTasks();

  // Read the final part of the data.
  auto data3 = stream_creator.ReadAllPendingData();
  EXPECT_TRUE(IsAllNulls(data3));
  EXPECT_EQ(data1.size() + data2.size() + data3.size(), kChunkSize);

  // Now the write() should settle.
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  // Nothing should be left to read.
  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre());
}

// Writing immediately followed by closing should not lose data.
TEST(TCPWritableStreamWrapperTest, WriteThenClose) {
  V8TestingScope scope;
  StreamCreator stream_creator;

  auto* tcp_writable_stream_wrapper = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create("D", 1);
  ScriptPromise write_promise =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);

  ScriptPromise close_promise =
      writer->close(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(scope.GetScriptState(), write_promise);
  ScriptPromiseTester close_tester(scope.GetScriptState(), close_promise);

  // Make sure that write() and close() both run before the event loop is
  // serviced.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  write_tester.WaitUntilSettled();
  ASSERT_TRUE(write_tester.IsFulfilled());
  close_tester.WaitUntilSettled();
  ASSERT_TRUE(close_tester.IsFulfilled());

  EXPECT_THAT(stream_creator.ReadAllPendingData(), ElementsAre('D'));
}

TEST(TCPWritableStreamWrapperTest, TriggerHasAborted) {
  V8TestingScope scope;
  StreamCreator stream_creator;
  EXPECT_FALSE(stream_creator.HasAborted());

  auto* tcp_writable_stream_wrapper = stream_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto* writer = tcp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);
  auto* chunk = DOMArrayBuffer::Create("D", 1);
  ScriptPromise write_promise =
      writer->write(script_state, ScriptValue::From(script_state, chunk),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(scope.GetScriptState(), write_promise);
  // Trigger onAborted() on purpose.
  stream_creator.ClosePipe();
  write_tester.WaitUntilSettled();

  EXPECT_TRUE(stream_creator.HasAborted());
}

}  // namespace

}  // namespace blink
