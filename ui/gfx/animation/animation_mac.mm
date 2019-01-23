// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/message_loop/message_loop.h"

// Only available since 10.12.
@interface NSWorkspace (AvailableSinceSierra)
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@end

namespace gfx {

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  // Because of sandboxing, OS settings should only be queried from the browser
  // process.
  DCHECK(base::MessageLoopCurrentForUI::IsSet() ||
         base::MessageLoopCurrentForIO::IsSet());

  bool enabled = false;
  id value = nil;
  value = [[NSUserDefaults standardUserDefaults]
      objectForKey:@"NSScrollAnimationEnabled"];
  if (value)
    enabled = [value boolValue];
  return enabled;
}

// static
bool Animation::PrefersReducedMotion() {
  // Because of sandboxing, OS settings should only be queried from the browser
  // process.
  DCHECK(base::MessageLoopCurrentForUI::IsSet() ||
         base::MessageLoopCurrentForIO::IsSet());

  // We default to assuming that animations are enabled, to avoid impacting the
  // experience for users on pre-10.12 systems.
  bool prefers_reduced_motion = false;
  SEL sel = @selector(accessibilityDisplayShouldReduceMotion);
  if ([[NSWorkspace sharedWorkspace] respondsToSelector:sel]) {
    prefers_reduced_motion =
        [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
  }
  return prefers_reduced_motion;
}

} // namespace gfx
