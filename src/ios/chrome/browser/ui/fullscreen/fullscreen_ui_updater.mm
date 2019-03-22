// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

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

void FullscreenUIUpdater::FullscreenEnabledStateChanged(
    FullscreenController* controller,
    bool enabled) {
  [ui_element_ updateForFullscreenEnabled:enabled];
}

void FullscreenUIUpdater::FullscreenWillAnimate(
    FullscreenController* controller,
    FullscreenAnimator* animator) {
  [ui_element_ animateFullscreenWithAnimator:animator];
}
