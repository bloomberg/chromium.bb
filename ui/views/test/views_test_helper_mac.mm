// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsautorelease_pool.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/event_generator_delegate_mac.h"
#include "ui/views/widget/widget.h"

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

void ViewsTestHelperMac::TearDown() {
  // Ensure all Widgets are closed explicitly in tests. The Widget may be
  // hosting a Compositor. If that's torn down after the test ContextFactory
  // then a lot of confusing use-after-free errors result. In browser tests,
  // this is handled automatically by views::Widget::CloseAllSecondaryWidgets().
  // Unit tests on Aura may create Widgets owned by a RootWindow that gets torn
  // down, but on Mac we need to be more explicit.
  base::mac::ScopedNSAutoreleasePool pool;  // Ensure the NSArray is released.
  NSArray* native_windows = [NSApp windows];
  for (NSWindow* window : native_windows)
    DCHECK(!Widget::GetWidgetForNativeWindow(window)) << "Widget not closed.";
}

}  // namespace views
