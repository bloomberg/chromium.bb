// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/coordinator.h"

#include <memory>

#include "base/run_loop.h"
#include "services/tracing/coordinator_test_util.h"
#include "services/tracing/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class CoordinatorTest : public testing::Test, public CoordinatorTestUtil {
 public:
  void SetUp() override {
    CoordinatorTestUtil::SetUp();
    coordinator_ = std::make_unique<Coordinator>(agent_registry(),
                                                 base::RepeatingClosure());
    coordinator_->FinishedReceivingRunningPIDs();
  }
  void TearDown() override { CoordinatorTestUtil::TearDown(); }
};

TEST_F(CoordinatorTest, StartTracingSimple) {
  base::RunLoop run_loop;
  auto* agent = AddArrayAgent();
  StartTracing("*", true);
  run_loop.RunUntilIdle();

  // The agent should have received exactly one call from the coordinator.
  EXPECT_EQ(1u, agent->call_stat().size());
  EXPECT_EQ("StartTracing", agent->call_stat()[0]);
}

TEST_F(CoordinatorTest, StartTracingTwoAgents) {
  base::RunLoop run_loop;
  auto* agent1 = AddArrayAgent();
  StartTracing("*", true);
  auto* agent2 = AddStringAgent();
  run_loop.RunUntilIdle();

  // Each agent should have received exactly one call from the coordinator.
  EXPECT_EQ(1u, agent1->call_stat().size());
  EXPECT_EQ("StartTracing", agent1->call_stat()[0]);
  EXPECT_EQ(1u, agent2->call_stat().size());
  EXPECT_EQ("StartTracing", agent2->call_stat()[0]);
}

TEST_F(CoordinatorTest, StartTracingWithProcessFilter) {
  base::RunLoop run_loop1;
  auto* agent1 = AddArrayAgent(static_cast<base::ProcessId>(1));
  auto* agent2 = AddArrayAgent(static_cast<base::ProcessId>(2));
  StartTracing("{\"included_process_ids\":[2,4]}", true);
  run_loop1.RunUntilIdle();

  base::RunLoop run_loop2;
  auto* agent3 = AddArrayAgent(static_cast<base::ProcessId>(3));
  auto* agent4 = AddArrayAgent(static_cast<base::ProcessId>(4));
  StartTracing("{\"included_process_ids\":[4,6]}", true);
  run_loop2.RunUntilIdle();

  base::RunLoop run_loop3;
  auto* agent5 = AddArrayAgent(static_cast<base::ProcessId>(5));
  auto* agent6 = AddArrayAgent(static_cast<base::ProcessId>(6));
  run_loop3.RunUntilIdle();

  // StartTracing should only be received by agents 2, 4, and 6.
  EXPECT_EQ(0u, agent1->call_stat().size());
  EXPECT_EQ(1u, agent2->call_stat().size());
  EXPECT_EQ("StartTracing", agent2->call_stat()[0]);
  EXPECT_EQ(0u, agent3->call_stat().size());
  EXPECT_EQ(1u, agent4->call_stat().size());
  EXPECT_EQ("StartTracing", agent4->call_stat()[0]);
  EXPECT_EQ(0u, agent5->call_stat().size());
  EXPECT_EQ(1u, agent6->call_stat().size());
  EXPECT_EQ("StartTracing", agent6->call_stat()[0]);
}

TEST_F(CoordinatorTest, StartTracingWithDifferentConfigs) {
  base::RunLoop run_loop;
  auto* agent = AddArrayAgent();
  StartTracing("config 1", true);
  // The 2nd |StartTracing| should return false.
  StartTracing("config 2", false);
  run_loop.RunUntilIdle();

  // The agent should have received exactly one call from the coordinator
  // because the 2nd |StartTracing| was aborted.
  EXPECT_EQ(1u, agent->call_stat().size());
  EXPECT_EQ("StartTracing", agent->call_stat()[0]);
}

TEST_F(CoordinatorTest, StartTracingWithSameConfigs) {
  base::RunLoop run_loop;
  auto* agent = AddArrayAgent();
  StartTracing("config", true);
  // The 2nd |StartTracing| should return true when we are not trying to change
  // the config.
  StartTracing("config", true);
  run_loop.RunUntilIdle();

  // The agent should have received exactly one call from the coordinator
  // because the 2nd |StartTracing| was a no-op.
  EXPECT_EQ(1u, agent->call_stat().size());
  EXPECT_EQ("StartTracing", agent->call_stat()[0]);
}

TEST_F(CoordinatorTest, StopAndFlushObjectAgent) {
  auto* agent = AddObjectAgent();
  agent->data_.push_back("\"content\":{\"a\":1}");
  agent->data_.push_back("\"name\":\"etw\"");

  StartTracing("config", true);
  std::string output = StopAndFlush();

  EXPECT_EQ("{\"systemTraceEvents\":{\"content\":{\"a\":1},\"name\":\"etw\"}}",
            output);

  // Each agent should have received exactly two calls.
  EXPECT_EQ(2u, agent->call_stat().size());
  EXPECT_EQ("StartTracing", agent->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent->call_stat()[1]);
}

TEST_F(CoordinatorTest, StopAndFlushTwoArrayAgents) {
  auto* agent1 = AddArrayAgent();
  agent1->data_.push_back("e1");
  agent1->data_.push_back("e2");

  auto* agent2 = AddArrayAgent();
  agent2->data_.push_back("e3");
  agent2->data_.push_back("e4");

  StartTracing("config", true);
  std::string output = StopAndFlush();

  // |output| should be of the form {"traceEvents":[ei,ej,ek,el]}, where
  // ei,ej,ek,el is a permutation of e1,e2,e3,e4 such that e1 is before e2 and
  // e3 is before e4 since e1 and 2 come from the same agent and their order
  // should be preserved and, similarly, the order of e3 and e4 should be
  // preserved, too.
  EXPECT_TRUE(output == "{\"traceEvents\":[e1,e2,e3,e4]}" ||
              output == "{\"traceEvents\":[e1,e3,e2,e4]}" ||
              output == "{\"traceEvents\":[e1,e3,e4,e2]}" ||
              output == "{\"traceEvents\":[e3,e1,e2,e4]}" ||
              output == "{\"traceEvents\":[e3,e1,e4,e2]}" ||
              output == "{\"traceEvents\":[e3,e4,e1,e2]}");

  // Each agent should have received exactly two calls.
  EXPECT_EQ(2u, agent1->call_stat().size());
  EXPECT_EQ("StartTracing", agent1->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent1->call_stat()[1]);

  EXPECT_EQ(2u, agent2->call_stat().size());
  EXPECT_EQ("StartTracing", agent2->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent2->call_stat()[1]);
}

TEST_F(CoordinatorTest, StopAndFlushDifferentTypeAgents) {
  auto* agent1 = AddArrayAgent();
  agent1->data_.push_back("e1");
  agent1->data_.push_back("e2");

  auto* agent2 = AddStringAgent();
  agent2->data_.push_back("e3");
  agent2->data_.push_back("e4");

  StartTracing("config", true);
  std::string output = StopAndFlush();

  EXPECT_TRUE(output == "{\"traceEvents\":[e1,e2],\"power\":\"e3e4\"}" ||
              output == "{\"power\":\"e3e4\",\"traceEvents\":[e1,e2]}");

  // Each agent should have received exactly two calls.
  EXPECT_EQ(2u, agent1->call_stat().size());
  EXPECT_EQ("StartTracing", agent1->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent1->call_stat()[1]);

  EXPECT_EQ(2u, agent2->call_stat().size());
  EXPECT_EQ("StartTracing", agent2->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent2->call_stat()[1]);
}

TEST_F(CoordinatorTest, StopAndFlushWithMetadata) {
  auto* agent = AddArrayAgent();
  agent->data_.push_back("event");
  agent->metadata_.SetString("key", "value");

  StartTracing("config", true);
  std::string output = StopAndFlush();

  // Metadata is written at after trace data.
  EXPECT_EQ("{\"traceEvents\":[event],\"metadata\":{\"key\":\"value\"}}",
            output);
  EXPECT_EQ(2u, agent->call_stat().size());
  EXPECT_EQ("StartTracing", agent->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent->call_stat()[1]);
}

TEST_F(CoordinatorTest, IsTracing) {
  base::RunLoop run_loop;
  AddArrayAgent();
  StartTracing("config", true);
  IsTracing(true);
  run_loop.RunUntilIdle();
}

TEST_F(CoordinatorTest, IsNotTracing) {
  base::RunLoop run_loop;
  IsTracing(false);
  run_loop.RunUntilIdle();
}

TEST_F(CoordinatorTest, RequestBufferUsage) {
  auto* agent1 = AddArrayAgent();
  agent1->trace_log_status_.event_capacity = 4;
  agent1->trace_log_status_.event_count = 1;
  RequestBufferUsage(0.25, 1);
  base::RunLoop().RunUntilIdle();
  CheckDisconnectClosures(1);

  auto* agent2 = AddArrayAgent();
  agent2->trace_log_status_.event_capacity = 8;
  agent2->trace_log_status_.event_count = 1;
  // The buffer usage of |agent2| is less than the buffer usage of |agent1| and
  // so the total buffer usage, i.e 0.25, does not change. But the approximate
  // count will be increased from 1 to 2.
  RequestBufferUsage(0.25, 2);
  base::RunLoop().RunUntilIdle();
  CheckDisconnectClosures(2);

  base::RunLoop run_loop3;
  auto* agent3 = AddArrayAgent();
  agent3->trace_log_status_.event_capacity = 8;
  agent3->trace_log_status_.event_count = 4;
  // |agent3| has the worst buffer usage of 0.5.
  RequestBufferUsage(0.5, 6);
  base::RunLoop().RunUntilIdle();
  CheckDisconnectClosures(3);

  // At the end |agent1| receives 3 calls, |agent2| receives 2 calls, and
  // |agent3| receives 1 call.
  EXPECT_EQ(3u, agent1->call_stat().size());
  EXPECT_EQ(2u, agent2->call_stat().size());
  EXPECT_EQ(1u, agent3->call_stat().size());
}

TEST_F(CoordinatorTest, LateAgents) {
  auto* agent1 = AddArrayAgent();

  StartTracing("config", true);
  StopAndFlush();

  base::RunLoop run_loop;
  auto* agent2 = AddArrayAgent();
  agent2->data_.push_back("discarded data");
  run_loop.RunUntilIdle();

  EXPECT_EQ(2u, agent1->call_stat().size());
  EXPECT_EQ("StartTracing", agent1->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent1->call_stat()[1]);

  // The second agent that registers after the end of a tracing session should
  // receive a StopAndFlush message.
  EXPECT_EQ(1u, agent2->call_stat().size());
  EXPECT_EQ("StopAndFlush", agent2->call_stat()[0]);
}

TEST_F(CoordinatorTest, WaitForSpecificPIDs) {
  coordinator_->AddExpectedPID(42);
  coordinator_->AddExpectedPID(4242);

  auto* agent1 = AddArrayAgent(42);
  StartTracing("config", true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracing_begin_callback_received());

  auto* agent2 = AddArrayAgent(4242);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracing_begin_callback_received());

  StopAndFlush();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, agent1->call_stat().size());
  EXPECT_EQ("StartTracing", agent1->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent1->call_stat()[1]);

  EXPECT_EQ(2u, agent2->call_stat().size());
  EXPECT_EQ("StartTracing", agent2->call_stat()[0]);
  EXPECT_EQ("StopAndFlush", agent2->call_stat()[1]);
}

}  // namespace tracing
