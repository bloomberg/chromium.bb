// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/help_view_controller.h"

// TODO(nicholss): These urls should come from a global config.
static NSString* const kHelpCenterUrl =
    @"https://support.google.com/chrome/answer/1649523?co=GENIE.Platform%3DiOS";

static NSString* const kCreditsUrlString =
    [[NSBundle mainBundle] URLForResource:@"credits" withExtension:@"html"]
        .absoluteString;

@implementation HelpViewController

- (instancetype)init {
  if (self = [super initWithUrl:kHelpCenterUrl title:@"Help Center"]) {
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithTitle:@"Credits"
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(onTapCredits:)];
  }
  return self;
}

#pragma mark - Private

- (void)onTapCredits:(id)button {
  WebViewController* creditsVC =
      [[WebViewController alloc] initWithUrl:kCreditsUrlString
                                       title:@"Credits"];
  [self.navigationController pushViewController:creditsVC animated:YES];
}

@end
