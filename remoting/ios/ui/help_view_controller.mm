// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/help_view_controller.h"

@implementation HelpViewController

// Override UIViewController
- (void)viewWillAppear:(BOOL)animated {
  [self.navigationController setNavigationBarHidden:NO animated:YES];
  NSString* string = @"https://support.google.com/chrome/answer/1649523";
  NSURL* url = [NSURL URLWithString:string];
  [_webView loadRequest:[NSURLRequest requestWithURL:url]];
}

@end
