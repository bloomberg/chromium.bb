// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_info.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(LatencyInfoTest, AddTwoSeparateEvent) {
  LatencyInfo info;
  info.set_trace_id(1);
  EXPECT_FALSE(info.began());
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                     0,
                                     1,
                                     base::TimeTicks::FromInternalValue(100),
                                     1);
  EXPECT_TRUE(info.began());
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                     1,
                                     5,
                                     base::TimeTicks::FromInternalValue(1000),
                                     2);

  EXPECT_EQ(info.latency_components().size(), 2u);
  LatencyInfo::LatencyComponent component;
  EXPECT_FALSE(
      info.FindLatency(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, &component));
  EXPECT_FALSE(
      info.FindLatency(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1, &component));
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 0, &component));
  EXPECT_EQ(component.sequence_number, 1);
  EXPECT_EQ(component.event_count, 1u);
  EXPECT_EQ(component.event_time.ToInternalValue(), 100);
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1, &component));
  EXPECT_EQ(component.sequence_number, 5);
  EXPECT_EQ(component.event_count, 2u);
  EXPECT_EQ(component.event_time.ToInternalValue(), 1000);
}

TEST(LatencyInfoTest, AddTwoSameEvent) {
  LatencyInfo info;
  info.set_trace_id(1);
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                     0,
                                     30,
                                     base::TimeTicks::FromInternalValue(100),
                                     2);
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                     0,
                                     13,
                                     base::TimeTicks::FromInternalValue(200),
                                     3);

  EXPECT_EQ(info.latency_components().size(), 1u);
  LatencyInfo::LatencyComponent component;
  EXPECT_FALSE(
      info.FindLatency(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, &component));
  EXPECT_FALSE(
      info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1, &component));
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, &component));
  EXPECT_EQ(component.sequence_number, 30);
  EXPECT_EQ(component.event_count, 5u);
  EXPECT_EQ(component.event_time.ToInternalValue(), (100 * 2 + 200 * 3) / 5);
}

TEST(LatencyInfoTest, RemoveLatency) {
  LatencyInfo info;
  info.set_trace_id(1);
  info.AddLatencyNumber(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, 0);
  info.AddLatencyNumber(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1, 0);
  info.AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);

  info.RemoveLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);

  EXPECT_FALSE(info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, 0));
  EXPECT_FALSE(info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1, 0));
  EXPECT_TRUE(info.FindLatency(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0));
}

}  // namespace ui
