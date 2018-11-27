// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this sink code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Web platform tests test WritableStream more thoroughly from scripts.
class WritableStreamTest : public testing::Test {
 public:
};

TEST_F(WritableStreamTest, CreateWithoutArguments) {
  V8TestingScope scope;

  WritableStream* stream =
      WritableStream::Create(scope.GetScriptState(), scope.GetExceptionState());
  ASSERT_TRUE(stream);
  ASSERT_FALSE(scope.GetExceptionState().HadException());
}

// Testing getWriter, locked and IsLocked.
TEST_F(WritableStreamTest, GetWriter) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  WritableStream* stream =
      WritableStream::Create(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(stream->locked(script_state, ASSERT_NO_EXCEPTION));
  EXPECT_EQ(stream->IsLocked(script_state, ASSERT_NO_EXCEPTION),
            base::make_optional(false));

  ScriptValue writer = stream->getWriter(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->locked(script_state, ASSERT_NO_EXCEPTION));
  EXPECT_EQ(stream->IsLocked(script_state, ASSERT_NO_EXCEPTION),
            base::make_optional(true));
}

}  // namespace

}  // namespace blink
