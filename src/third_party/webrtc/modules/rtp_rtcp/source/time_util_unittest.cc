/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/time_util.h"

#include "test/gtest.h"

namespace webrtc {

TEST(TimeUtilTest, CompactNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const NtpTime kNtp(kNtpSec, kNtpFrac);
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, CompactNtp(kNtp));
}

TEST(TimeUtilTest, CompactNtpRttToTimeDelta) {
  const NtpTime ntp1(0x12345, 0x23456);
  const NtpTime ntp2(0x12654, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);

  EXPECT_NEAR(CompactNtpRttToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpRttToTimeDeltaWithWrap) {
  const NtpTime ntp1(0x1ffff, 0x23456);
  const NtpTime ntp2(0x20000, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();

  // While ntp2 > ntp1, there compact ntp presentation happen to be opposite.
  // That shouldn't be a problem as long as unsigned arithmetic is used.
  ASSERT_GT(ntp2.ToMs(), ntp1.ToMs());
  ASSERT_LT(CompactNtp(ntp2), CompactNtp(ntp1));

  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_NEAR(CompactNtpRttToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpRttToTimeDeltaLarge) {
  const NtpTime ntp1(0x10000, 0x00006);
  const NtpTime ntp2(0x17fff, 0xffff5);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  // Ntp difference close to 2^15 seconds should convert correctly too.
  ASSERT_NEAR(ms_diff, ((1 << 15) - 1) * 1000, 1);
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_NEAR(CompactNtpRttToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpRttToTimeDeltaNegative) {
  const NtpTime ntp1(0x20000, 0x23456);
  const NtpTime ntp2(0x1ffff, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  ASSERT_GT(0, ms_diff);
  // Ntp difference close to 2^16 seconds should be treated as negative.
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_EQ(CompactNtpRttToTimeDelta(ntp_diff), TimeDelta::Millis(1));
}

TEST(TimeUtilTest, SaturatedToCompactNtp) {
  // Converts negative to zero.
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(-1)), 0u);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Zero()), 0u);
  // Converts values just above and just below max uint32_t.
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(65536000000)), 0xffffffff);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(65535999985)), 0xffffffff);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(65535999970)), 0xfffffffe);
  // Converts half-seconds.
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Millis(500)), 0x8000u);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Seconds(1)), 0x10000u);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Millis(1'500)), 0x18000u);
  // Convert us -> compact_ntp -> TimeDelta. Compact ntp precision is ~15us.
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Micros(1'516)))
          .us(),
      1'516, 16);
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Millis(15)))
          .us(),
      15'000, 16);
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Micros(5'485)))
          .us(),
      5'485, 16);
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Micros(5'515)))
          .us(),
      5'515, 16);
}

}  // namespace webrtc
