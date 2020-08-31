// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator+subclassing.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_presentation_state.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarCoordinator () <InfobarCoordinatorImplementation,
                                  InfobarBannerPositioner,
                                  InfobarModalPositioner> {
  // The AnimatedFullscreenDisable disables fullscreen by displaying the
  // Toolbar/s when an Infobar banner is presented.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;
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
// Readwrite redefinition.
@property(nonatomic, assign, readwrite) BOOL bannerWasPresented;
// YES if the banner is in the process of being dismissed.
@property(nonatomic, assign) BOOL bannerIsBeingDismissed;
// Completion block used to dismiss the banner after a set period of time. This
// needs to be created by dispatch_block_create() since it may get cancelled.
@property(nonatomic, copy) dispatch_block_t dismissBannerBlock;

@end

@implementation InfobarCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize baseViewController = _baseViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize browser = _browser;
// Property defined in InfobarUIDelegate.
@synthesize delegate = _delegate;
// Property defined in InfobarUIDelegate.
@synthesize hasBadge = _hasBadge;
// Property defined in InfobarUIDelegate.
@synthesize infobarType = _infobarType;
// Property defined in InfobarUIDelegate.
@synthesize presented = _presented;

- (instancetype)initWithInfoBarDelegate:
                    (infobars::InfoBarDelegate*)infoBarDelegate
                           badgeSupport:(BOOL)badgeSupport
                                   type:(InfobarType)infobarType {
  self = [super initWithBaseViewController:nil browser:nil];
  if (self) {
    _infobarDelegate = infoBarDelegate;
    _presented = YES;
    _hasBadge = badgeSupport;
    _infobarType = infobarType;
  }
  return self;
}

#pragma mark - Public Methods.

- (void)stop {
  _animatedFullscreenDisabler = nullptr;
  _badgeDelegate = nil;
  _infobarDelegate = nil;
}

- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion {
  DCHECK(self.browser);
  DCHECK(self.baseViewController);
  DCHECK(self.bannerViewController);
  DCHECK(self.started);

  // If |self.baseViewController| is not part of the ViewHierarchy the banner
  // shouldn't be presented.
  if (!self.baseViewController.view.window) {
    return;
  }

  // Make sure to display the Toolbar/s before presenting the Banner.
  if (fullscreen::features::ShouldScopeFullscreenControllerToBrowser()) {
    _animatedFullscreenDisabler =
        std::make_unique<AnimatedScopedFullscreenDisabler>(
            FullscreenController::FromBrowser(self.browser));
  } else {
    _animatedFullscreenDisabler =
        std::make_unique<AnimatedScopedFullscreenDisabler>(
            FullscreenController::FromBrowserState(
                self.browser->GetBrowserState()));
  }
  _animatedFullscreenDisabler->StartAnimation();

  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  if ([self.bannerViewController
          conformsToProtocol:@protocol(InfobarBannerInteractable)]) {
    UIViewController<InfobarBannerInteractable>* interactableBanner =
        base::mac::ObjCCastStrict<UIViewController<InfobarBannerInteractable>>(
            self.bannerViewController);
    interactableBanner.interactionDelegate = self.bannerTransitionDriver;
  }

  self.infobarBannerState = InfobarBannerPresentationState::IsAnimating;
  [self.baseViewController
      presentViewController:self.bannerViewController
                   animated:animated
                 completion:^{
                   // Capture self in order to make sure the animation dismisses
                   // correctly in case the Coordinator gets stopped mid
                   // presentation. This will also make sure some cleanup tasks
                   // like configuring accessibility for the presenter VC are
                   // performed successfully.
                   [self configureAccessibilityForBannerInViewController:
                             self.baseViewController
                                                              presenting:YES];
                   self.bannerWasPresented = YES;
                   // Set to NO for each Banner this coordinator might present.
                   self.bannerIsBeingDismissed = NO;
                   self.infobarBannerState =
                       InfobarBannerPresentationState::Presented;
                   [self.badgeDelegate
                       infobarBannerWasPresented:self.infobarType
                                     forWebState:self.webState];
                   [self infobarBannerWasPresented];
                   if (completion)
                     completion();
                 }];

  // Dismisses the presented banner after a certain number of seconds.
  if (!UIAccessibilityIsVoiceOverRunning()) {
    NSTimeInterval timeInterval =
        self.highPriorityPresentation
            ? kInfobarBannerLongPresentationDurationInSeconds
            : kInfobarBannerDefaultPresentationDurationInSeconds;
    dispatch_time_t popTime =
        dispatch_time(DISPATCH_TIME_NOW, timeInterval * NSEC_PER_SEC);
    if (self.dismissBannerBlock) {
      // TODO:(crbug.com/1021805): Write unittest to cover this situation.
      dispatch_block_cancel(self.dismissBannerBlock);
    }
    __weak InfobarCoordinator* weakSelf = self;
    self.dismissBannerBlock =
        dispatch_block_create(DISPATCH_BLOCK_ASSIGN_CURRENT, ^{
          [weakSelf dismissInfobarBannerIfReady];
          weakSelf.dismissBannerBlock = nil;
        });
    dispatch_after(popTime, dispatch_get_main_queue(), self.dismissBannerBlock);
  }
}

- (void)presentInfobarModal {
  DCHECK(self.started);
  ProceduralBlock modalPresentation = ^{
    DCHECK(self.infobarBannerState !=
           InfobarBannerPresentationState::Presented);
    DCHECK(self.baseViewController);
    self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
        initWithTransitionMode:InfobarModalTransitionBase];
    self.modalTransitionDriver.modalPositioner = self;
    __weak __typeof(self) weakSelf = self;
    [self presentInfobarModalFrom:self.baseViewController
                           driver:self.modalTransitionDriver
                       completion:^{
                         [weakSelf infobarModalPresentedFromBanner:NO];
                       }];
  };

  // Dismiss InfobarBanner first if being presented.
  if (self.baseViewController.presentedViewController &&
      self.baseViewController.presentedViewController ==
          self.bannerViewController) {
    [self dismissInfobarBannerAnimated:NO completion:modalPresentation];
  } else {
    modalPresentation();
  }
}

#pragma mark - Protocols

#pragma mark InfobarUIDelegate

- (void)removeView {
  // Do not animate the dismissal since the Coordinator might have been stopped
  // and the animation can cause undefined behavior.
  [self dismissInfobarBannerAnimated:NO completion:nil];
}

- (void)detachView {
  // Do not animate the dismissals since the Coordinator might have been stopped
  // and the animation can cause undefined behavior.
  if (self.bannerViewController)
    [self dismissInfobarBannerAnimated:NO completion:nil];
  if (self.modalViewController)
    [self dismissInfobarModalAnimated:NO];
  [self stop];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  if (!self.infobarDelegate)
    return;

  [self performInfobarAction];
  // The Infobar action might be async, and the badge should not change until
  // the Infobar has been accepted.
  if ([self isInfobarAccepted]) {
    [self.badgeDelegate infobarWasAccepted:self.infobarType
                               forWebState:self.webState];
  }
  // If the Banner Button will present the Modal then the banner shouldn't be
  // dismissed.
  if (![self infobarBannerActionWillPresentModal]) {
    [self dismissInfobarBannerAnimated:YES completion:nil];
  }
}

- (void)presentInfobarModalFromBanner {
  DCHECK(self.bannerViewController);
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalTransitionDriver.modalPositioner = self;
  __weak __typeof(self) weakSelf = self;
  [self presentInfobarModalFrom:self.bannerViewController
                         driver:self.modalTransitionDriver
                     completion:^{
                       [weakSelf infobarModalPresentedFromBanner:YES];
                     }];
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  [self dismissInfobarBannerAnimated:YES
                       userInitiated:userInitiated
                          completion:nil];
}

- (void)infobarBannerWasDismissed {
  DCHECK(self.infobarBannerState == InfobarBannerPresentationState::Presented);

  self.infobarBannerState = InfobarBannerPresentationState::NotPresented;
  [self configureAccessibilityForBannerInViewController:self.baseViewController
                                             presenting:NO];
  [self.badgeDelegate infobarBannerWasDismissed:self.infobarType
                                    forWebState:self.webState];
  self.bannerTransitionDriver = nil;
  _animatedFullscreenDisabler = nullptr;
  [self infobarWasDismissed];
  if (!self.infobarActionInProgress) {
    // Only inform InfobarContainer that the Infobar banner presentation is
    // finished if it is not still executing the Infobar action. That way, the
    // container won't start presenting a queued Infobar's banner when the
    // current Infobar hasn't finished.
    [self.infobarContainer childCoordinatorBannerFinishedPresented:self];
  }
}

#pragma mark InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  NamedGuide* omniboxGuide =
      [NamedGuide guideWithName:kOmniboxGuide
                           view:self.baseViewController.view];
  UIView* omniboxView = omniboxGuide.owningView;
  CGRect omniboxFrame = [omniboxView convertRect:omniboxGuide.layoutFrame
                                          toView:omniboxView.window];
  return CGRectGetMaxY(omniboxFrame) - kInfobarBannerOverlapWithOmnibox;
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self performInfobarAction];
  if ([self isInfobarAccepted]) {
    [self.badgeDelegate infobarWasAccepted:self.infobarType
                               forWebState:self.webState];
  }
  [self dismissInfobarModalAnimated:YES];
}

- (void)dismissInfobarModal:(id)infobarModal {
  [self dismissInfobarModalAnimated:YES];
}

- (void)modalInfobarWasDismissed:(id)infobarModal {
  self.modalTransitionDriver = nil;

  // If InfobarBanner is being presented it means that this Modal was presented
  // by an InfobarBanner. If this is the case InfobarBanner will call
  // infobarWasDismissed and clean up once it gets dismissed, this prevents
  // counting the dismissal metrics twice.
  if (self.infobarBannerState != InfobarBannerPresentationState::Presented)
    [self infobarWasDismissed];
}

#pragma mark InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  return [self infobarModalHeightForWidth:width];
}

#pragma mark InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (BOOL)isInfobarAccepted {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (BOOL)infobarBannerActionWillPresentModal {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (void)infobarBannerWasPresented {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  NOTREACHED() << "Subclass must implement.";
}

- (void)dismissBannerIfReady {
  NOTREACHED() << "Subclass must implement.";
}

- (BOOL)infobarActionInProgress {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (void)performInfobarAction {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarWasDismissed {
  NOTREACHED() << "Subclass must implement.";
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  NOTREACHED() << "Subclass must implement.";
  return 0;
}

#pragma mark - Private

// Dismisses the Infobar banner if it is ready. i.e. the user is no longer
// interacting with it or the Infobar action is still in progress. The dismissal
// will be animated.
- (void)dismissInfobarBannerIfReady {
  if (!self.modalTransitionDriver) {
    [self dismissBannerIfReady];
  }
}

// |presentingViewController| presents the InfobarModal using |driver|. If
// Modal is presented successfully |completion| will be executed.
- (void)presentInfobarModalFrom:(UIViewController*)presentingViewController
                         driver:(InfobarModalTransitionDriver*)driver
                     completion:(ProceduralBlock)completion {
  // |self.modalViewController| only exists while one its being presented, if
  // this is the case early return since there's one already being presented.
  if (self.modalViewController)
    return;

  BOOL infobarWasConfigured = [self configureModalViewController];
  if (!infobarWasConfigured) {
    if (driver.transitionMode == InfobarModalTransitionBanner) {
      [self dismissInfobarBannerAnimated:NO completion:nil];
    }
    return;
  }

  DCHECK(self.modalViewController);
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = driver;
  navController.modalPresentationStyle = UIModalPresentationCustom;
  [presentingViewController presentViewController:navController
                                         animated:YES
                                       completion:completion];
}

// Configures the Banner Accessibility in order to give VoiceOver users the
// ability to select other elements while the banner is presented. Call this
// method after the Banner has been presented or dismissed. |presenting| is YES
// if banner was presented, NO if dismissed.
- (void)configureAccessibilityForBannerInViewController:
            (UIViewController*)presentingViewController
                                             presenting:(BOOL)presenting {
  if (presenting) {
    UpdateBannerAccessibilityForPresentation(presentingViewController,
                                             self.bannerViewController.view);
  } else {
    UpdateBannerAccessibilityForDismissal(presentingViewController);
  }
}

#pragma mark - Dismissal Helpers

// Helper method for non-user initiated InfobarBanner dismissals.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion {
  [self dismissInfobarBannerAnimated:animated
                       userInitiated:NO
                          completion:completion];
}

// Helper for banner dismissals.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                       userInitiated:(BOOL)userInitiated
                          completion:(void (^)())completion {
  DCHECK(self.baseViewController);
  // Make sure the banner is completely presented before trying to dismiss it.
  [self.bannerTransitionDriver completePresentationTransitionIfRunning];

  // The banner dismiss can be triggered concurrently due to different events
  // like swiping it up, entering the TabSwitcher, presenting another VC or the
  // InfobarDelelgate being destroyed. Trying to dismiss it twice might cause a
  // UIKit crash on iOS12.
  if (!self.bannerIsBeingDismissed &&
      self.bannerViewController.presentingViewController) {
    self.bannerIsBeingDismissed = YES;
    [self infobarBannerWillBeDismissed:userInitiated];
    [self.bannerViewController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:completion];
  } else if (completion) {
    completion();
  }
}

- (void)dismissInfobarModalAnimated:(BOOL)animated {
  [self dismissInfobarModalAnimated:animated completion:nil];
}

@end

@implementation InfobarCoordinator (Subclassing)

- (void)dismissInfobarModalAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion {
  DCHECK(self.baseViewController);
  UIViewController* presentedViewController =
      self.baseViewController.presentedViewController;
  if (!presentedViewController) {
    if (completion)
      completion();
    return;
  }
  // If the Modal is being presented by the Banner, call dismiss on it.
  // This way the modal dismissal will animate correctly and the completion
  // block cleans up the banner correctly.
  if (self.baseViewController.presentedViewController ==
      self.bannerViewController) {
    __weak __typeof(self) weakSelf = self;
    [self.bannerViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [weakSelf dismissInfobarBannerAnimated:NO
                                                         completion:completion];
                           }];

  } else if (presentedViewController ==
             self.modalViewController.navigationController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:^{
                                                  if (completion)
                                                    completion();
                                                }];
  }
}

@end
