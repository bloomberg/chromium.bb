// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_win.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(AudioCapturerWinTest, SilenceTest) {
  int16 samples1[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int16 samples2[] = {65, 73, 83, 89, 92, -1, 5, 9, 123, 0};
  int16 samples3[] = {0, 0, 0, 0, 1, 0, 0, -1, 0, 0};
  // MSVC++ doesn't allow allocating arrays of size 0.
  int16 samples4[] = {0};

  ASSERT_TRUE(
      AudioCapturerWin::IsPacketOfSilence(samples1, arraysize(samples1)));
  ASSERT_FALSE(
      AudioCapturerWin::IsPacketOfSilence(samples2, arraysize(samples2)));
  ASSERT_TRUE(
      AudioCapturerWin::IsPacketOfSilence(samples3, arraysize(samples3)));
  ASSERT_TRUE(
      AudioCapturerWin::IsPacketOfSilence(samples4, 0));
}

}  // namespace remoting
