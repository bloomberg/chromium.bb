// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"
#import "ios/chrome/browser/ui/util/named_guide.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Banner View constants.
const CGFloat kBannerOverlapWithOmnibox = 5.0;
}  // namespace

@interface InfobarCoordinator () <InfobarCoordinatorImplementation,
                                  InfobarBannerPositioner> {
  // The AnimatedFullscreenDisable disables fullscreen by displaying the
  // Toolbar/s when an Infobar banner is presented.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> animatedFullscreenDisabler_;
}

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly) infobars::InfoBarDelegate* infobarDelegate;
// The transition delegate used by the Coordinator to present the InfobarBanner.
// nil if no Banner is being presented.
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
// The transition delegate used by the Coordinator to present the InfobarModal.
// nil if no Modal is being presented.
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;

@end

@implementation InfobarCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize baseViewController = _baseViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize browserState = _browserState;
// Property defined in InfobarUIDelegate.
@synthesize delegate = _delegate;
// Property defined in InfobarUIDelegate.
@synthesize presented = _presented;

- (instancetype)initWithInfoBarDelegate:
    (infobars::InfoBarDelegate*)infoBarDelegate {
  self = [super initWithBaseViewController:nil browserState:nil];
  if (self) {
    _infobarDelegate = infoBarDelegate;
    _presented = YES;
  }
  return self;
}

#pragma mark - Public Methods.

- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion {
  DCHECK(self.browserState);
  DCHECK(self.baseViewController);
  DCHECK(self.bannerViewController);

  // Make sure to display the Toolbar/s before presenting the Banner.
  animatedFullscreenDisabler_ =
      std::make_unique<AnimatedScopedFullscreenDisabler>(
          FullscreenControllerFactory::GetInstance()->GetForBrowserState(
              self.browserState));
  animatedFullscreenDisabler_->StartAnimation();

  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  [self.baseViewController presentViewController:self.bannerViewController
                                        animated:animated
                                      completion:completion];
}

- (void)presentInfobarModal {
  ProceduralBlock modalPresentation = ^{
    DCHECK(!self.bannerViewController);
    DCHECK(self.baseViewController);
    self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
        initWithTransitionMode:InfobarModalTransitionBase];
    [self presentInfobarModalFrom:self.baseViewController
                           driver:self.modalTransitionDriver];
  };

  // Dismiss InfobarBanner first if being presented.
  if (self.baseViewController.presentedViewController) {
    [self dismissInfobarBanner:self animated:NO completion:modalPresentation];
  } else {
    modalPresentation();
  }
}

- (void)dismissInfobarBannerAfterInteraction {
  if (!self.modalTransitionDriver) {
    [self dismissBannerWhenInteractionIsFinished];
  }
}

- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion {
  [self dismissInfobarBanner:self animated:animated completion:completion];
}

#pragma mark - Protocols

#pragma mark InfobarUIDelegate

- (void)removeView {
  [self dismissInfobarBanner:self animated:YES completion:nil];
}

- (void)detachView {
  [self dismissInfobarBanner:self animated:YES completion:nil];
  [self stop];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self performInfobarAction];
  [self.badgeDelegate infobarWasAccepted];
  [self dismissInfobarBanner:sender animated:YES completion:nil];
}

- (void)presentInfobarModalFromBanner {
  DCHECK(self.bannerViewController);
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  [self presentInfobarModalFrom:self.bannerViewController
                         driver:self.modalTransitionDriver];
}

- (void)dismissInfobarBanner:(id)sender
                    animated:(BOOL)animated
                  completion:(void (^)())completion {
  DCHECK(self.baseViewController);
  if (self.baseViewController.presentedViewController) {
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [weakSelf.badgeDelegate infobarBannerWasDismissed];
                             weakSelf.bannerTransitionDriver = nil;
                             animatedFullscreenDisabler_ = nullptr;
                             [weakSelf infobarWasDismissed];
                             if (completion)
                               completion();
                           }];
  } else {
    if (completion)
      completion();
  }
}

#pragma mark InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  NamedGuide* topLayout =
      [NamedGuide guideWithName:kOmniboxGuide
                           view:self.baseViewController.view];
  return topLayout.constrainedView.frame.origin.y +
         topLayout.constrainedView.frame.size.height -
         kBannerOverlapWithOmnibox;
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasPressed:(UIButton*)sender {
  [self performInfobarAction];
  [self.badgeDelegate infobarWasAccepted];
  [self dismissInfobarModal:sender completion:nil];
}

- (void)dismissInfobarModal:(UIButton*)sender
                 completion:(void (^)())completion {
  DCHECK(self.baseViewController);
  if (self.baseViewController.presentedViewController) {
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf.badgeDelegate infobarModalWasDismissed];
                             weakSelf.modalTransitionDriver = nil;
                             [weakSelf infobarWasDismissed];
                             if (completion)
                               completion();
                           }];
  } else {
    if (completion)
      completion();
  }
}

#pragma mark InfobarCoordinatorImplementation

- (void)configureModalViewController {
  NOTREACHED() << "Subclass must implement.";
}

- (void)dismissBannerWhenInteractionIsFinished {
  NOTREACHED() << "Subclass must implement.";
}

- (void)performInfobarAction {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarWasDismissed {
  NOTREACHED() << "Subclass must implement.";
}

#pragma mark - Private

- (void)presentInfobarModalFrom:(UIViewController*)presentingViewController
                         driver:(InfobarModalTransitionDriver*)driver {
  [self configureModalViewController];
  DCHECK(self.modalViewController);
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = driver;
  navController.modalPresentationStyle = UIModalPresentationCustom;
  [presentingViewController presentViewController:navController
                                         animated:YES
                                       completion:nil];
  [self.badgeDelegate infobarModalWasPresented];
}

@end
