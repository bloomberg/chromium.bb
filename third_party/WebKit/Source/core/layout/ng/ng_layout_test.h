// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutTest_h
#define NGLayoutTest_h

#include "core/frame/LocalFrameClient.h"
#include "core/testing/CoreUnitTestHelper.h"
#include "platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

// The NGLayoutTest is intended to let all NGFooTest classes easiy inherit
// ScopedLayoutNGForTest as well as RenderingTest. The ScopedLayoutNGForTest
// ensures original settings are restored for other tests.
// See http://crbug.com/769541 for more details.
class NGLayoutTest : public RenderingTest, private ScopedLayoutNGForTest {
 public:
  NGLayoutTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client), ScopedLayoutNGForTest(true) {}
};

}  // namespace blink

#endif  // NGLayoutTest_h
