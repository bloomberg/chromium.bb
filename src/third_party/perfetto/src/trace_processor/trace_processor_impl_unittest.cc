/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/trace_processor_impl.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(TraceProcessorImplTest, GuessTraceType_Empty) {
  const uint8_t prefix[] = "";
  EXPECT_EQ(kUnknownTraceType, GuessTraceType(prefix, 0));
}

TEST(TraceProcessorImplTest, GuessTraceType_Json) {
  const uint8_t prefix[] = "{\"traceEvents\":[";
  EXPECT_EQ(kJsonTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_JsonWithSpaces) {
  const uint8_t prefix[] = "\n{ \"traceEvents\": [";
  EXPECT_EQ(kJsonTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_JsonMissingTraceEvents) {
  const uint8_t prefix[] = "[{";
  EXPECT_EQ(kJsonTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

TEST(TraceProcessorImplTest, GuessTraceType_Proto) {
  const uint8_t prefix[] = {0x0a, 0x65, 0x18, 0x8f, 0x4e, 0x32, 0x60, 0x0a};
  EXPECT_EQ(kProtoTraceType, GuessTraceType(prefix, sizeof(prefix)));
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
