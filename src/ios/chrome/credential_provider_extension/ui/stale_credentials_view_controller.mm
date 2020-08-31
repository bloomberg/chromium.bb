// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/stale_credentials_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kStackViewSpacingAfterIllustration = 32;
}  // namespace

@implementation StaleCredentialsViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"empty_credentials_illustration"];
  self.customSpacingAfterImage = kStackViewSpacingAfterIllustration;

  self.helpButtonAvailable = NO;
  self.primaryActionAvailable = NO;
  NSString* titleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_STALE_CREDENTIALS_TITLE",
                        @"The title in the stale credentials screen.");
  NSString* subtitleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_STALE_CREDENTIALS_SUBTITLE",
      @"The subtitle in the stale credentials screen.");
  self.titleString = titleString;
  self.subtitleString = subtitleString;

#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    self.pointerInteractionEnabled = YES;
  }
#endif  // defined(__IPHONE_13_4)

  [super loadView];
}

@end
