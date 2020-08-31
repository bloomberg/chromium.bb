// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripLegacyCoordinator ()
@property(nonatomic, assign) BOOL started;
@property(nonatomic, strong) TabStripController* tabStripController;
@end

@implementation TabStripLegacyCoordinator
@synthesize longPressDelegate = _longPressDelegate;
@synthesize presentationProvider = _presentationProvider;
@synthesize started = _started;
@synthesize tabStripController = _tabStripController;
@synthesize animationWaitDuration = _animationWaitDuration;

- (void)setLongPressDelegate:(id<PopupMenuLongPressDelegate>)longPressDelegate {
  _longPressDelegate = longPressDelegate;
  self.tabStripController.longPressDelegate = longPressDelegate;
}

- (UIView*)view {
  DCHECK(self.started);
  return [self.tabStripController view];
}

- (void)setPresentationProvider:(id<TabStripPresentation>)presentationProvider {
  DCHECK(!self.started);
  _presentationProvider = presentationProvider;
}

- (void)setAnimationWaitDuration:(NSTimeInterval)animationWaitDuration {
  DCHECK(!self.started);
  _animationWaitDuration = animationWaitDuration;
}

- (void)setHighlightsSelectedTab:(BOOL)highlightsSelectedTab {
  DCHECK(self.started);
  self.tabStripController.highlightsSelectedTab = highlightsSelectedTab;
}

- (void)hideTabStrip:(BOOL)hidden {
  [self.tabStripController hideTabStrip:hidden];
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.presentationProvider);
  TabStripStyle style =
      self.browser->GetBrowserState()->IsOffTheRecord() ? INCOGNITO : NORMAL;
  self.tabStripController =
      [[TabStripController alloc] initWithBrowser:self.browser style:style];
  self.tabStripController.presentationProvider = self.presentationProvider;
  self.tabStripController.animationWaitDuration = self.animationWaitDuration;
  self.tabStripController.longPressDelegate = self.longPressDelegate;
  [self.presentationProvider showTabStripView:[self.tabStripController view]];
  self.started = YES;
}

- (void)stop {
  self.started = NO;
  [self.tabStripController disconnect];
  self.tabStripController = nil;
  self.presentationProvider = nil;
}

@end
