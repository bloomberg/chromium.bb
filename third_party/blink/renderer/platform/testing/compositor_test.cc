// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/compositor_test.h"

namespace blink {

CompositorTest::CompositorTest()
    : runner_(new base::TestMockTimeTaskRunner), runner_handle_(runner_) {}

CompositorTest::~CompositorTest() = default;

}  // namespace blink
