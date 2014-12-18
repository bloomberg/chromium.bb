// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/event_generator_delegate_mac.h"

namespace views {

// static
ViewsTestHelper* ViewsTestHelper::Create(base::MessageLoopForUI* message_loop,
                                         ui::ContextFactory* context_factory) {
  return new ViewsTestHelperMac;
}

ViewsTestHelperMac::ViewsTestHelperMac()
    : zero_duration_mode_(new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION)) {
  test::InitializeMacEventGeneratorDelegate();

  // Unbundled applications (those without Info.plist) default to
  // NSApplicationActivationPolicyProhibited, which prohibits the application
  // obtaining key status or activating windows without user interaction.
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

ViewsTestHelperMac::~ViewsTestHelperMac() {
}

}  // namespace views
