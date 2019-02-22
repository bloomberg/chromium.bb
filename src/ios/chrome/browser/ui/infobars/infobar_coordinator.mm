// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_coordinator.h"

#include <memory>

#include "base/ios/block_types.h"
#include "ios/chrome/browser/infobars/infobar_container_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_container_view.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarCoordinator ()<InfobarContainerStateDelegate> {
  // Bridge class to deliver container change notifications to BVC.
  std::unique_ptr<InfoBarContainerDelegateIOS> _infoBarContainerDelegate;

  // A single infobar container handles all infobars in all tabs. It keeps
  // track of infobars for current tab (accessed via infobar helper of
  // the current tab).
  std::unique_ptr<InfoBarContainerIOS> _infoBarContainer;
}

@end

@implementation InfobarCoordinator

- (nullable instancetype)
initWithBaseViewController:(nullable UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _infoBarContainerDelegate.reset(new InfoBarContainerDelegateIOS(self));
    _infoBarContainer.reset(
        new InfoBarContainerIOS(_infoBarContainerDelegate.get()));
  }
  return self;
}

#pragma mark - Public Interface

- (UIView*)view {
  return _infoBarContainer->view();
}

- (void)restoreInfobars {
  _infoBarContainer->RestoreInfobars();
}

- (void)suspendInfobars {
  _infoBarContainer->SuspendInfobars();
}

- (void)setInfobarManager:(infobars::InfoBarManager*)infoBarManager {
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

- (void)updateInfobarContainer {
  [self infoBarContainerStateDidChangeAnimated:NO];
}

#pragma mark - InfobarContainerStateDelegate

- (void)infoBarContainerStateDidChangeAnimated:(BOOL)animated {
  InfoBarContainerView* infoBarContainerView = _infoBarContainer->view();
  DCHECK(infoBarContainerView);
  CGRect containerFrame = infoBarContainerView.frame;
  CGFloat height = [infoBarContainerView topmostVisibleInfoBarHeight];
  containerFrame.origin.y =
      CGRectGetMaxY([self.positioner parentView].frame) - height;
  containerFrame.size.height = height;

  BOOL isViewVisible = [self.positioner isParentViewVisible];
  ProceduralBlock frameUpdates = ^{
    [infoBarContainerView setFrame:containerFrame];
  };
  void (^completion)(BOOL) = ^(BOOL finished) {
    if (!isViewVisible)
      return;
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    infoBarContainerView);
  };
  if (animated) {
    [UIView animateWithDuration:0.1
                     animations:frameUpdates
                     completion:completion];
  } else {
    frameUpdates();
    completion(YES);
  }
}

@end
