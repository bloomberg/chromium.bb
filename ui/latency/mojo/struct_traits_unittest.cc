// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/latency/mojo/latency_info_struct_traits.h"
#include "ui/latency/mojo/traits_test_service.mojom.h"

namespace ui {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoLatencyComponent(
      const LatencyInfo::LatencyComponent& l,
      const EchoLatencyComponentCallback& callback) override {
    callback.Run(l);
  }

  void EchoLatencyComponentId(
      const std::pair<LatencyComponentType, int64_t>& id,
      const EchoLatencyComponentIdCallback& callback) override {
    callback.Run(id);
  }

  void EchoLatencyInfo(const LatencyInfo& info,
                       const EchoLatencyInfoCallback& callback) override {
    callback.Run(info);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, LatencyComponent) {
  const int64_t sequence_number = 13371337;
  const base::TimeTicks event_time = base::TimeTicks::Now();
  const uint32_t event_count = 1234;
  LatencyInfo::LatencyComponent input;
  input.sequence_number = sequence_number;
  input.event_time = event_time;
  input.event_count = event_count;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  LatencyInfo::LatencyComponent output;
  proxy->EchoLatencyComponent(input, &output);
  EXPECT_EQ(sequence_number, output.sequence_number);
  EXPECT_EQ(event_time, output.event_time);
  EXPECT_EQ(event_count, output.event_count);
}

TEST_F(StructTraitsTest, LatencyComponentId) {
  const LatencyComponentType type =
      INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
  const int64_t id = 1337;
  std::pair<LatencyComponentType, int64_t> input(type, id);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::pair<LatencyComponentType, int64_t> output;
  proxy->EchoLatencyComponentId(input, &output);
  EXPECT_EQ(type, output.first);
  EXPECT_EQ(id, output.second);
}

TEST_F(StructTraitsTest, LatencyInfo) {
  LatencyInfo latency;
  ASSERT_FALSE(latency.terminated());
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1234, 0);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1234, 100);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT,
                           1234, 0);

  EXPECT_EQ(100, latency.trace_id());
  EXPECT_TRUE(latency.terminated());

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  LatencyInfo output;
  proxy->EchoLatencyInfo(latency, &output);

  EXPECT_EQ(latency.trace_id(), output.trace_id());
  EXPECT_EQ(latency.terminated(), output.terminated());

  EXPECT_TRUE(output.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1234,
                                 nullptr));

  LatencyInfo::LatencyComponent rwh_comp;
  EXPECT_TRUE(output.FindLatency(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1234,
                                 &rwh_comp));
  EXPECT_EQ(100, rwh_comp.sequence_number);
  EXPECT_EQ(1u, rwh_comp.event_count);

  EXPECT_TRUE(output.FindLatency(
      INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 1234, nullptr));
}

}  // namespace ui
