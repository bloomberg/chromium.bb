// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>
#include <iostream>
#include <thread>

#include "absl/types/optional.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#define TRACE_FORCE_ENABLE true

#include "platform/api/trace_logging.h"

// TODO(issue/52): Remove duplicate code from trace logging+internal unit tests
namespace openscreen {
namespace platform {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;

class MockLoggingPlatform : public TraceLoggingPlatform {
 public:
  MOCK_METHOD9(LogTrace,
               void(const char*,
                    const uint32_t,
                    const char* file,
                    Clock::time_point,
                    Clock::time_point,
                    TraceId,
                    TraceId,
                    TraceId,
                    Error::Code));
  MOCK_METHOD7(LogAsyncStart,
               void(const char*,
                    const uint32_t,
                    const char* file,
                    Clock::time_point,
                    TraceId,
                    TraceId,
                    TraceId));
  MOCK_METHOD5(LogAsyncEnd,
               void(const uint32_t,
                    const char* file,
                    Clock::time_point,
                    TraceId,
                    Error::Code));
};

// Methods to validate the results of platform-layer calls.
template <uint64_t milliseconds>
void ValidateTraceTimestampDiff(const char* name,
                                const uint32_t line,
                                const char* file,
                                Clock::time_point start_time,
                                Clock::time_point end_time,
                                TraceId trace_id,
                                TraceId parent_id,
                                TraceId root_id,
                                Error error) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  EXPECT_GE(static_cast<uint64_t>(elapsed.count()), milliseconds);
}

template <Error::Code result>
void ValidateTraceErrorCode(const char* name,
                            const uint32_t line,
                            const char* file,
                            Clock::time_point start_time,
                            Clock::time_point end_time,
                            TraceId trace_id,
                            TraceId parent_id,
                            TraceId root_id,
                            Error error) {
  EXPECT_EQ(error.code(), result);
}

TEST(TraceLoggingTest, MacroCallScopedDoesntSegFault) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(1);
  { TRACE_SCOPED(TraceCategory::Value::Any, "test"); }
}

TEST(TraceLoggingTest, MacroCallUnscopedDoesntSegFault) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _)).Times(1);
  { TRACE_ASYNC_START(TraceCategory::Value::Any, "test"); }
}

TEST(TraceLoggingTest, MacroVariablesUniquelyNames) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(2);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _)).Times(2);

  {
    TRACE_SCOPED(TraceCategory::Value::Any, "test1");
    TRACE_SCOPED(TraceCategory::Value::Any, "test2");
    TRACE_ASYNC_START(TraceCategory::Value::Any, "test3");
    TRACE_ASYNC_START(TraceCategory::Value::Any, "test4");
  }
}

TEST(TraceLoggingTest, ExpectTimestampsReflectDelay) {
  constexpr uint32_t delay_in_ms = 50;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(Invoke(ValidateTraceTimestampDiff<delay_in_ms>),
                      Invoke(ValidateTraceErrorCode<Error::Code::kNone>)));

  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_in_ms));
  }
}

TEST(TraceLoggingTest, ExpectErrorsPassedToResult) {
  constexpr Error::Code result_code = Error::Code::kParseError;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<result_code>));

  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    TRACE_SET_RESULT(result_code);
  }
}

TEST(TraceLoggingTest, ExpectUnsetTraceIdNotSet) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(1);

  TraceIdHierarchy h = {kUnsetTraceId, kUnsetTraceId, kUnsetTraceId};
  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", h);

    auto ids = TRACE_HIERARCHY;
    EXPECT_NE(ids.current, kUnsetTraceId);
    EXPECT_NE(ids.parent, kUnsetTraceId);
    EXPECT_NE(ids.root, kUnsetTraceId);
  }
}

TEST(TraceLoggingTest, ExpectCreationWithIdsToWork) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, current, parent, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TraceIdHierarchy h = {current, parent, root};
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", h);

    auto ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    EXPECT_EQ(TRACE_CURRENT_ID, current);
    EXPECT_EQ(TRACE_ROOT_ID, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToBeApplied) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, current, parent, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TraceIdHierarchy h = {current, parent, root};
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", h);
    auto ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    ids = TRACE_HIERARCHY;
    EXPECT_NE(ids.current, current);
    EXPECT_EQ(ids.parent, current);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToEndAfterScopeWhenSetWithSetter) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SET_HIERARCHY(ids);
    {
      TRACE_SCOPED(TraceCategory::Value::Any, "Name");
      ids = TRACE_HIERARCHY;
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToEndAfterScope) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, current, parent, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", ids);
    {
      TRACE_SCOPED(TraceCategory::Value::Any, "Name");
      ids = TRACE_HIERARCHY;
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectSetHierarchyToApply) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SET_HIERARCHY(ids);
    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    ids = TRACE_HIERARCHY;
    EXPECT_NE(ids.current, current);
    EXPECT_EQ(ids.parent, current);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, CheckTraceAsyncStartLogsCorrectly) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _)).Times(1);

  { TRACE_ASYNC_START(TraceCategory::Value::Any, "Name"); }
}

TEST(TraceLoggingTest, CheckTraceAsyncStartSetsHierarchy) {
  constexpr TraceId current = 32;
  constexpr TraceId parent = 47;
  constexpr TraceId root = 84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, current, root)).Times(1);

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SET_HIERARCHY(ids);
    {
      TRACE_ASYNC_START(TraceCategory::Value::Any, "Name");
      ids = TRACE_HIERARCHY;
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, CheckTraceAsyncEndLogsCorrectly) {
  constexpr TraceId id = 12345;
  constexpr Error::Code result = Error::Code::kAgain;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncEnd(_, _, _, id, result)).Times(1);

  TRACE_ASYNC_END(TraceCategory::Value::Any, id, result);
}

}  // namespace platform
}  // namespace openscreen

#undef TRACE_FORCE_ENABLE
