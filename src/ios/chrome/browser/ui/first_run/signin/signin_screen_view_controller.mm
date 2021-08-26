// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Width of the identity control if nothing is contraining it.
const CGFloat kIdentityControlMaxWidth = 327;
const CGFloat kIdentityTopMargin = 16;
}  // namespace

@interface SigninScreenViewController ()

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;

// The string to be displayed in the "Cotinue" button to personalize it. Usually
// the given name, or the email address if no given name.
@property(nonatomic, copy) NSString* personalizedButtonPrompt;

@end

@implementation SigninScreenViewController
@dynamic delegate;

#pragma mark - Public

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunSignInScreenAccessibilityIdentifier;
  self.bannerImage = [UIImage imageNamed:@"signin_screen_banner"];
  self.isTallBanner = NO;
  self.scrollToEndMandatory = YES;

  self.titleText = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE);
  if (!self.primaryActionString) {
    // |primaryActionString| could already be set using the consumer methods.
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION);
  }
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN);

  [self.specificContentView addSubview:self.identityControl];

  NSLayoutConstraint* widthConstraint = [self.identityControl.widthAnchor
      constraintEqualToConstant:kIdentityControlMaxWidth];
  widthConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [self.identityControl.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor
                       constant:kIdentityTopMargin],
    [self.identityControl.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.identityControl.widthAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView.widthAnchor],
    widthConstraint,
    [self.identityControl.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

#pragma mark - Properties

- (IdentityButtonControl*)identityControl {
  if (!_identityControl) {
    _identityControl = [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
    _identityControl.translatesAutoresizingMaskIntoConstraints = NO;
    [_identityControl addTarget:self
                         action:@selector(identityButtonControlTapped:forEvent:)
               forControlEvents:UIControlEventTouchUpInside];

    // Setting the content hugging priority isn't working, so creating a
    // low-priority constraint to make sure that the view is as small as
    // possible.
    NSLayoutConstraint* heightConstraint =
        [_identityControl.heightAnchor constraintEqualToConstant:0];
    heightConstraint.priority = UILayoutPriorityDefaultLow - 1;
    heightConstraint.active = YES;
  }
  return _identityControl;
}

#pragma mark - SignInScreenConsumer

- (void)setUserImage:(UIImage*)userImage {
  [self.identityControl setIdentityAvatar:userImage];
}

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName {
  self.personalizedButtonPrompt = givenName ? givenName : email;
  [self updateUIForIdentityAvailable:YES];
  [self.identityControl setIdentityName:userName email:email];
}

- (void)noIdentityAvailable {
  [self updateUIForIdentityAvailable:NO];
}

#pragma mark - Private

// Callback for |identityControl|.
- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate showAccountPickerFromPoint:[touch locationInView:nil]];
}

// Updates the UI to adapt for |identityAvailable| or not.
- (void)updateUIForIdentityAvailable:(BOOL)identityAvailable {
  self.identityControl.hidden = !identityAvailable;
  if (identityAvailable) {
    self.primaryActionString = l10n_util::GetNSStringF(
        IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
        base::SysNSStringToUTF16(self.personalizedButtonPrompt));
    ;
  } else {
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION);
  }
}

@end
