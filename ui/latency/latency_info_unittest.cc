// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_info.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks ToTestTimeTicks(int64_t micros) {
  return base::TimeTicks() + base::TimeDelta::FromMicroseconds(micros);
}

}  // namespace

TEST(LatencyInfoTest, AddTwoSeparateEvent) {
  LatencyInfo info;
  info.set_trace_id(1);
  EXPECT_FALSE(info.began());
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                     ToTestTimeTicks(100), 1);
  EXPECT_TRUE(info.began());
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                     ToTestTimeTicks(1000), 2);

  EXPECT_EQ(info.latency_components().size(), 2u);
  LatencyInfo::LatencyComponent component;
  EXPECT_FALSE(info.FindLatency(INPUT_EVENT_LATENCY_UI_COMPONENT, &component));
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &component));
  EXPECT_EQ(component.event_count, 1u);
  EXPECT_EQ(component.event_time, ToTestTimeTicks(100));
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, &component));
  EXPECT_EQ(component.event_count, 2u);
  EXPECT_EQ(component.event_time, ToTestTimeTicks(1000));
}

}  // namespace ui
