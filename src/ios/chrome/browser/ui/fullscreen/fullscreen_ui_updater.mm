// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenUIUpdater::FullscreenUIUpdater(id<FullscreenUIElement> ui_element)
    : ui_element_(ui_element) {}

void FullscreenUIUpdater::FullscreenProgressUpdated(
    FullscreenController* controller,
    CGFloat progress) {
  [ui_element_ updateForFullscreenProgress:progress];
}

void FullscreenUIUpdater::FullscreenViewportInsetRangeChanged(
    FullscreenController* controller,
    UIEdgeInsets min_viewport_insets,
    UIEdgeInsets max_viewport_insets) {
  SEL inset_range_selector = @selector(updateForFullscreenMinViewportInsets:
                                                          maxViewportInsets:);
  if ([ui_element_ respondsToSelector:inset_range_selector]) {
    [ui_element_ updateForFullscreenMinViewportInsets:min_viewport_insets
                                    maxViewportInsets:max_viewport_insets];
  }
}

void FullscreenUIUpdater::FullscreenEnabledStateChanged(
    FullscreenController* controller,
    bool enabled) {
  if ([ui_element_ respondsToSelector:@selector(updateForFullscreenEnabled:)]) {
    [ui_element_ updateForFullscreenEnabled:enabled];
  } else if (!enabled) {
    [ui_element_ updateForFullscreenProgress:1.0];
  }
}

void FullscreenUIUpdater::FullscreenWillAnimate(
    FullscreenController* controller,
    FullscreenAnimator* animator) {
  SEL animator_selector = @selector(animateFullscreenWithAnimator:);
  if ([ui_element_ respondsToSelector:animator_selector]) {
    [ui_element_ animateFullscreenWithAnimator:animator];
  } else {
    CGFloat progress = animator.finalProgress;
    [animator addAnimations:^{
      [ui_element_ updateForFullscreenProgress:progress];
    }];
  }
}
