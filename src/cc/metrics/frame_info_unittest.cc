// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using MainThreadResponse = FrameInfo::MainThreadResponse;
using FrameFinalState = FrameInfo::FrameFinalState;

TEST(FrameInfoTest, WasMainUpdateDroppedForkedReporters) {
  FrameInfo main, compositor;

  main.main_thread_response = MainThreadResponse::kIncluded;
  compositor.main_thread_response = MainThreadResponse::kMissing;

  // For a 'forked' frame: the compositor-update is dropped, and the main-thread
  // update was presented. For this frame, both the compositor-thread and the
  // main-thread update are considered dropped (because the compositor-thread
  // did not update any new main-thread).
  main.final_state = FrameFinalState::kPresentedAll;
  compositor.final_state = FrameFinalState::kDropped;

  auto test = main;
  test.MergeWith(compositor);
  EXPECT_TRUE(test.WasMainUpdateDropped());
  EXPECT_TRUE(test.WasCompositorUpdateDropped());

  // Even if the compositor-thread update is presented, the overall main-thread
  // update is dropped if the compositor-update is not accompanied by new
  // main-thread update (from earlier frames).
  compositor.final_state = FrameFinalState::kPresentedPartialOldMain;
  test = main;
  test.MergeWith(compositor);
  EXPECT_TRUE(test.WasMainUpdateDropped());
  EXPECT_FALSE(test.WasCompositorUpdateDropped());

  // If the compositor-thread is accompanied by new main-thread update (even if
  // from earlier frames), then the main-thread is update is considered not
  // dropped.
  compositor.final_state = FrameFinalState::kPresentedPartialNewMain;
  test = main;
  test.MergeWith(compositor);
  EXPECT_FALSE(test.WasMainUpdateDropped());
  EXPECT_FALSE(test.WasCompositorUpdateDropped());
}

}  // namespace
}  // namespace cc
