// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter.h"

#include "base/check.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller_delegate.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMinHeight = 200;
const CGFloat kMinWidth = 200;
const CGFloat kMaxWidth = 300;
const CGFloat kMaxHeight = 435;
const CGFloat kMinWidthDifference = 50;
const CGFloat kMinHorizontalMargin = 5;
const CGFloat kMinVerticalMargin = 15;
const CGFloat kDamping = 0.85;
}  // namespace

@interface PopupMenuPresenter () <PopupMenuViewControllerDelegate>
@property(nonatomic, strong) PopupMenuViewController* popupViewController;
// Constraints used for the initial positioning of the popup.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* initialConstraints;
// Constraints used for the positioning of the popup when presented.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* presentedConstraints;
@end

@implementation PopupMenuPresenter

@synthesize baseViewController = _baseViewController;
@synthesize delegate = _delegate;
@synthesize guideName = _guideName;
@synthesize popupViewController = _popupViewController;
@synthesize initialConstraints = _initialConstraints;
@synthesize presentedConstraints = _presentedConstraints;
@synthesize presentedViewController = _presentedViewController;

#pragma mark - Public

- (void)prepareForPresentation {
  DCHECK(self.baseViewController);
  if (self.popupViewController)
    return;

  self.popupViewController = [[PopupMenuViewController alloc] init];
  self.popupViewController.delegate = self;
  [self.presentedViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Set the frame of the table view to the maximum width to have the label
  // resizing correctly.
  CGRect frame = self.presentedViewController.view.frame;
  frame.size.width = kMaxWidth;
  self.presentedViewController.view.frame = frame;
  // It is necessary to do a first layout pass so the table view can size
  // itself.
  [self.presentedViewController.view setNeedsLayout];
  [self.presentedViewController.view layoutIfNeeded];
  CGSize fittingSize = [self.presentedViewController.view
      sizeThatFits:CGSizeMake(kMaxWidth, kMaxHeight)];
  // Use preferredSize if it is set.
  CGSize preferredSize = self.presentedViewController.preferredContentSize;
  CGFloat width = fittingSize.width;
  CGFloat height = fittingSize.height;
  if (!CGSizeEqualToSize(preferredSize, CGSizeZero)) {
    width = preferredSize.width;
    height = preferredSize.height;
  }

  // Set the sizing constraints, in case the UIViewController is using a
  // UIScrollView. The priority needs to be non-required to allow downsizing if
  // needed, and more than UILayoutPriorityDefaultHigh to take precedence on
  // compression resistance.
  NSLayoutConstraint* widthConstraint =
      [self.presentedViewController.view.widthAnchor
          constraintEqualToConstant:width];
  widthConstraint.priority = UILayoutPriorityDefaultHigh + 1;

  NSLayoutConstraint* heightConstraint =
      [self.presentedViewController.view.heightAnchor
          constraintEqualToConstant:height];
  heightConstraint.priority = UILayoutPriorityDefaultHigh + 1;

  UIView* popup = self.popupViewController.contentContainer;
  [NSLayoutConstraint activateConstraints:@[
    widthConstraint,
    heightConstraint,
    [popup.heightAnchor constraintLessThanOrEqualToConstant:kMaxHeight],
    [popup.widthAnchor constraintLessThanOrEqualToConstant:kMaxWidth],
    [popup.widthAnchor constraintGreaterThanOrEqualToConstant:kMinWidth],
  ]];
  [self.popupViewController addContent:self.presentedViewController];

  [self.baseViewController addChildViewController:self.popupViewController];
  [self.baseViewController.view addSubview:self.popupViewController.view];
  self.popupViewController.view.frame = self.baseViewController.view.bounds;

  [popup.widthAnchor constraintLessThanOrEqualToAnchor:self.popupViewController
                                                           .view.widthAnchor
                                              constant:-kMinWidthDifference]
      .active = YES;

  UILayoutGuide* namedGuide =
      [NamedGuide guideWithName:self.guideName
                           view:self.baseViewController.view];
  self.initialConstraints = @[
    [popup.centerXAnchor constraintEqualToAnchor:namedGuide.centerXAnchor],
    [popup.centerYAnchor constraintEqualToAnchor:namedGuide.centerYAnchor],
  ];
  [self setUpPresentedConstraints];

  // Configure the initial state of the animation.
  popup.alpha = 0;
  popup.transform = CGAffineTransformMakeScale(0.1, 0.1);
  [NSLayoutConstraint activateConstraints:self.initialConstraints];
  [self.baseViewController.view layoutIfNeeded];

  [self.popupViewController
      didMoveToParentViewController:self.baseViewController];
}

- (void)presentAnimated:(BOOL)animated {
  [NSLayoutConstraint deactivateConstraints:self.initialConstraints];
  [NSLayoutConstraint activateConstraints:self.presentedConstraints];
  [self
      animate:^{
        self.popupViewController.contentContainer.alpha = 1;
        [self.baseViewController.view layoutIfNeeded];
        self.popupViewController.contentContainer.transform =
            CGAffineTransformIdentity;
      }
      withCompletion:^(BOOL finished) {
        if ([self.delegate
                respondsToSelector:@selector(containedPresenterDidPresent:)]) {
          [self.delegate containedPresenterDidPresent:self];
        }
      }];
}

- (void)dismissAnimated:(BOOL)animated {
  [self.popupViewController willMoveToParentViewController:nil];
  [NSLayoutConstraint deactivateConstraints:self.presentedConstraints];
  [NSLayoutConstraint activateConstraints:self.initialConstraints];
  auto completion = ^(BOOL finished) {
    [self.popupViewController.view removeFromSuperview];
    [self.popupViewController removeFromParentViewController];
    self.popupViewController = nil;
    if ([self.delegate
            respondsToSelector:@selector(containedPresenterDidDismiss:)]) {
      [self.delegate containedPresenterDidDismiss:self];
    }
  };
  if (animated) {
    [self
               animate:^{
                 self.popupViewController.contentContainer.alpha = 0;
                 [self.baseViewController.view layoutIfNeeded];
                 self.popupViewController.contentContainer.transform =
                     CGAffineTransformMakeScale(0.1, 0.1);
               }
        withCompletion:completion];
  } else {
    completion(YES);
  }
}

#pragma mark - Private

// Animate the |animations| then execute |completion|.
- (void)animate:(void (^)(void))animation
    withCompletion:(void (^)(BOOL finished))completion {
  [UIView animateWithDuration:ios::material::kDuration1
                        delay:0
       usingSpringWithDamping:kDamping
        initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:animation
                   completion:completion];
}

// Sets |presentedConstraints| up, such as they are positioning the popup
// relatively to the |guideName| layout guide. The popup is positioned closest
// to the layout guide, by default it is presented below the layout guide,
// aligned on its leading edge. However, it is respecting the safe area bounds.
- (void)setUpPresentedConstraints {
  UIView* parentView = self.baseViewController.view;
  UIView* container = self.popupViewController.contentContainer;

  UILayoutGuide* namedGuide = [NamedGuide guideWithName:self.guideName
                                                   view:parentView];
  CGRect guideFrame =
      [self.popupViewController.view convertRect:namedGuide.layoutFrame
                                        fromView:namedGuide.owningView];

  NSLayoutConstraint* verticalPositioning = nil;
  if (CGRectGetMaxY(guideFrame) + kMinHeight >
      CGRectGetHeight(parentView.frame)) {
    // Display above.
    verticalPositioning =
        [container.bottomAnchor constraintEqualToAnchor:namedGuide.topAnchor];
  } else {
    // Display below.
    verticalPositioning =
        [container.topAnchor constraintEqualToAnchor:namedGuide.bottomAnchor];
  }

  NSLayoutConstraint* center = [container.centerXAnchor
      constraintEqualToAnchor:namedGuide.centerXAnchor];
  center.priority = UILayoutPriorityDefaultHigh;

  id<LayoutGuideProvider> safeArea = parentView.safeAreaLayoutGuide;
  self.presentedConstraints = @[
    center,
    verticalPositioning,
    [container.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.leadingAnchor
                                    constant:kMinHorizontalMargin],
    [container.trailingAnchor
        constraintLessThanOrEqualToAnchor:safeArea.trailingAnchor
                                 constant:-kMinHorizontalMargin],
    [container.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeArea.bottomAnchor
                                 constant:-kMinVerticalMargin],
    [container.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.topAnchor
                                    constant:kMinVerticalMargin],
  ];
}

#pragma mark - PopupMenuViewControllerDelegate

- (void)popupMenuViewControllerWillDismiss:
    (PopupMenuViewController*)viewController {
  [self.delegate popupMenuPresenterWillDismiss:self];
}

@end
