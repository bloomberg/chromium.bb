// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_mac.h"

#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace views {

// static
ViewsTestHelper* ViewsTestHelper::Create(base::MessageLoopForUI* message_loop,
                                         ui::ContextFactory* context_factory) {
  return new ViewsTestHelperMac;
}

ViewsTestHelperMac::ViewsTestHelperMac()
    : zero_duration_mode_(new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION)) {
}

ViewsTestHelperMac::~ViewsTestHelperMac() {
}

}  // namespace views
