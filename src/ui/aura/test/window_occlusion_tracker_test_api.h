// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_OCCLUSION_TRACKER_TEST_API_H_
#define UI_AURA_TEST_WINDOW_OCCLUSION_TRACKER_TEST_API_H_

#include "base/macros.h"

namespace aura {

class Env;
class WindowOcclusionTracker;

namespace test {

class WindowOcclusionTrackerTestApi {
 public:
  explicit WindowOcclusionTrackerTestApi(Env* env);
  ~WindowOcclusionTrackerTestApi();

  // Returns the number of times that occlusion was recomputed in this process.
  int GetNumTimesOcclusionRecomputed() const;

 private:
  WindowOcclusionTracker* const tracker_;
  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionTrackerTestApi);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_WINDOW_OCCLUSION_TRACKER_TEST_API_H_
