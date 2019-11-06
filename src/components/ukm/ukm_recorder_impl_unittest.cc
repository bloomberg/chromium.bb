// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include "base/bind.h"
#include "base/metrics/ukm_source_id.h"
#include "base/test/scoped_feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace ukm {

TEST(UkmRecorderImplTest, IsSampledIn) {
  UkmRecorderImpl impl;

  for (int i = 0; i < 100; ++i) {
    // These are constant regardless of the seed, source, and event.
    EXPECT_FALSE(impl.IsSampledIn(-i, i, 0));
    EXPECT_TRUE(impl.IsSampledIn(-i, i, 1));
  }

  // These depend on the source, event, and initial seed. There's no real
  // predictability here but should see roughly 50% true and 50% false with
  // no obvious correlation and the same for every run of the test.
  impl.SetSamplingSeedForTesting(123);
  EXPECT_FALSE(impl.IsSampledIn(1, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(1, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 2, 2));
  impl.SetSamplingSeedForTesting(456);
  EXPECT_TRUE(impl.IsSampledIn(1, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(1, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 2, 2));
  impl.SetSamplingSeedForTesting(789);
  EXPECT_TRUE(impl.IsSampledIn(1, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(1, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(2, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(2, 2, 2));
  EXPECT_FALSE(impl.IsSampledIn(3, 1, 2));
  EXPECT_TRUE(impl.IsSampledIn(3, 2, 2));
  EXPECT_TRUE(impl.IsSampledIn(4, 1, 2));
  EXPECT_FALSE(impl.IsSampledIn(4, 2, 2));
}

}  // namespace ukm
