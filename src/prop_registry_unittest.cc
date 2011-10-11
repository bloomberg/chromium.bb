// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/prop_registry.h"

namespace gestures {

class PropRegistryTest : public ::testing::Test {};

class PropRegistryTestDelegate : public PropertyDelegate {
 public:
  PropRegistryTestDelegate() : call_cnt_(0) {}
  virtual void BoolWasWritten(BoolProperty* prop) { call_cnt_++; };
  virtual void DoubleWasWritten(DoubleProperty* prop) { call_cnt_++; };
  virtual void IntWasWritten(IntProperty* prop) { call_cnt_++; }
  virtual void ShortWasWritten(ShortProperty* prop) { call_cnt_++; }
  virtual void StringWasWritten(StringProperty* prop) { call_cnt_++; }

  int call_cnt_;
};

TEST(PropRegistryTest, SimpleTest) {
  PropRegistry reg;
  PropRegistryTestDelegate delegate;

  int expected_call_cnt = 0;
  BoolProperty bp1(&reg, "hi", false, &delegate);
  EXPECT_TRUE(strstr(bp1.Value().c_str(), "false"));
  bp1.HandleGesturesPropWritten();
  EXPECT_EQ(++expected_call_cnt, delegate.call_cnt_);

  BoolProperty bp2(&reg, "hi", true);
  EXPECT_TRUE(strstr(bp2.Value().c_str(), "true"));
  bp2.HandleGesturesPropWritten();
  EXPECT_EQ(expected_call_cnt, delegate.call_cnt_);

  DoubleProperty dp1(&reg, "hi", 2.0, &delegate);
  EXPECT_TRUE(strstr(dp1.Value().c_str(), "2"));
  dp1.HandleGesturesPropWritten();
  EXPECT_EQ(++expected_call_cnt, delegate.call_cnt_);

  DoubleProperty dp2(&reg, "hi", 3.1);
  EXPECT_TRUE(strstr(dp2.Value().c_str(), "3.1"));
  dp2.HandleGesturesPropWritten();
  EXPECT_EQ(expected_call_cnt, delegate.call_cnt_);

  IntProperty ip1(&reg, "hi", 567, &delegate);
  EXPECT_TRUE(strstr(ip1.Value().c_str(), "567"));
  ip1.HandleGesturesPropWritten();
  EXPECT_EQ(++expected_call_cnt, delegate.call_cnt_);

  IntProperty ip2(&reg, "hi", 568);
  EXPECT_TRUE(strstr(ip2.Value().c_str(), "568"));
  ip2.HandleGesturesPropWritten();
  EXPECT_EQ(expected_call_cnt, delegate.call_cnt_);

  ShortProperty sp1(&reg, "hi", 234, &delegate);
  EXPECT_TRUE(strstr(sp1.Value().c_str(), "234"));
  sp1.HandleGesturesPropWritten();
  EXPECT_EQ(++expected_call_cnt, delegate.call_cnt_);

  ShortProperty sp2(&reg, "hi", 235);
  EXPECT_TRUE(strstr(sp2.Value().c_str(), "235"));
  sp2.HandleGesturesPropWritten();
  EXPECT_EQ(expected_call_cnt, delegate.call_cnt_);

  StringProperty stp1(&reg, "hi", "foo", &delegate);
  EXPECT_TRUE(strstr(stp1.Value().c_str(), "foo"));
  stp1.HandleGesturesPropWritten();
  EXPECT_EQ(++expected_call_cnt, delegate.call_cnt_);

  StringProperty stp2(&reg, "hi", "bar");
  EXPECT_TRUE(strstr(stp2.Value().c_str(), "bar"));
  stp2.HandleGesturesPropWritten();
  EXPECT_EQ(expected_call_cnt, delegate.call_cnt_);
}

}  // namespace gestures
