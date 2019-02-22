// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_coordinator.h"

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShortcutsCoordinator ()

// Redefined as readwrite and as ShortcutsViewController.
@property(nonatomic, strong, readwrite) ShortcutsViewController* viewController;

@end

@implementation ShortcutsCoordinator

- (void)start {
  self.viewController = [[ShortcutsViewController alloc] init];
}

- (void)stop {
  self.viewController = nil;
}

@end
