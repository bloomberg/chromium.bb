// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/window_occlusion_tracker_test_api.h"

#include "ui/aura/env.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace aura {
namespace test {

WindowOcclusionTrackerTestApi::WindowOcclusionTrackerTestApi(Env* env)
    : tracker_(env->GetWindowOcclusionTracker()) {}

WindowOcclusionTrackerTestApi::~WindowOcclusionTrackerTestApi() = default;

int WindowOcclusionTrackerTestApi::GetNumTimesOcclusionRecomputed() const {
  return tracker_->num_times_occlusion_recomputed_;
}

}  // namespace test
}  // namespace aura
