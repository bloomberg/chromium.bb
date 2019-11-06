// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"

#include <string>

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/ntp/incognito_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoViewController ()
// The scrollview containing the actual views.
@property(nonatomic, strong) IncognitoView* incognitoView;
@end

@implementation IncognitoViewController {
  // The UrlLoadingService associated with this view.
  UrlLoadingService* _urlLoadingService;  // weak
}

- (id)initWithUrlLoadingService:(UrlLoadingService*)urlLoadingService {
  self = [super init];
  if (self) {
    _urlLoadingService = urlLoadingService;
  }
  return self;
}

- (void)viewDidLoad {
  self.incognitoView = [[IncognitoView alloc] initWithFrame:self.view.bounds
                                          urlLoadingService:_urlLoadingService];
  [self.incognitoView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleWidth];

  [self.incognitoView
      setBackgroundColor:[UIColor colorWithWhite:34 / 255.0 alpha:1.0]];

  [self.view addSubview:self.incognitoView];
}

- (void)dealloc {
  [_incognitoView setDelegate:nil];
}

@end
