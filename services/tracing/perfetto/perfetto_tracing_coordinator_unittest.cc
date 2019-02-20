// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"

#include "base/json/json_reader.h"
#include "services/tracing/coordinator_test_util.h"
#include "services/tracing/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class PerfettoCoordinatorTest : public testing::Test,
                                public CoordinatorTestUtil {
 public:
  void SetUp() override {
    CoordinatorTestUtil::SetUp();
    coordinator_ = std::make_unique<PerfettoTracingCoordinator>(
        agent_registry(), base::RepeatingClosure());
    coordinator_->FinishedReceivingRunningPIDs();
  }
  void TearDown() override { CoordinatorTestUtil::TearDown(); }
};

TEST_F(PerfettoCoordinatorTest, TraceBufferSizeInBytes) {
  auto* agent = AddArrayAgent();
  agent->data_.push_back("e1");

  StartTracing("{\"trace_buffer_size_in_kb\":4}", true);
  std::string output = StopAndFlush();

  auto json_value = base::JSONReader::Read(output);
  ASSERT_TRUE(json_value);

  const base::DictionaryValue* dict = nullptr;
  json_value->GetAsDictionary(&dict);
  ASSERT_TRUE(dict->GetDictionary("metadata", &dict));
  ASSERT_TRUE(dict->GetDictionary("perfetto_trace_stats", &dict));
  const base::ListValue* list = nullptr;
  ASSERT_TRUE(dict->GetList("buffer_stats", &list));
  EXPECT_EQ(1u, list->GetSize());
  ASSERT_TRUE(list->GetDictionary(0, &dict));
  int buffer_size = 0;
  ASSERT_TRUE(dict->GetInteger("buffer_size", &buffer_size));
  EXPECT_EQ(4096, buffer_size);
}

}  // namespace tracing
