// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/web_view_controller.h"

#import <WebKit/WebKit.h>

#import "remoting/ios/app/remoting_theme.h"

@interface WebViewController () {
  NSString* _urlString;
}
@end

@implementation WebViewController

- (instancetype)initWithUrl:(NSString*)url title:(NSString*)title {
  if (self = [super init]) {
    _urlString = url;
    self.title = title;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  WKWebView* webView =
      [[WKWebView alloc] initWithFrame:CGRectZero
                         configuration:[[WKWebViewConfiguration alloc] init]];
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:_urlString]];
  [webView loadRequest:request];
  self.view = webView;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Show a close button if it is the first view in the navigation stack.
  UIViewController* firstViewController =
      [self.navigationController.viewControllers firstObject];
  if ((firstViewController == self ||
       firstViewController == self.parentViewController) &&
      !self.navigationItem.leftBarButtonItem &&
      !self.navigationItem.leftBarButtonItems.count) {
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithImage:RemotingTheme.closeIcon
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(didTapClose:)];
  }
}

#pragma mark - Private

- (void)didTapClose:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
