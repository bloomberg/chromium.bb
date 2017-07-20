// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/side_menu_items.h"

#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/remoting_theme.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static NSString* const kFeedbackContext = @"SideMenuFeedbackContext";

#pragma mark - SideMenuItem

@interface SideMenuItem ()

- (instancetype)initWithTitle:(NSString*)title
                         icon:(UIImage*)icon
                       action:(SideMenuItemAction)action;

@end

@implementation SideMenuItem

@synthesize title = _title;
@synthesize icon = _icon;
@synthesize action = _action;

- (instancetype)initWithTitle:(NSString*)title
                         icon:(UIImage*)icon
                       action:(SideMenuItemAction)action {
  _title = title;
  _icon = icon;
  _action = action;
  return self;
}

@end

#pragma mark - SideMenuItemsProvider

@implementation SideMenuItemsProvider

+ (NSArray<NSArray<SideMenuItem*>*>*)sideMenuItems {
  static NSArray<NSArray<SideMenuItem*>*>* items = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    items = @[ @[
      [[SideMenuItem alloc]
          initWithTitle:l10n_util::GetNSString(IDS_ACTIONBAR_SEND_FEEDBACK)
                   icon:RemotingTheme.feedbackIcon
                 action:^{
                   [AppDelegate.instance
                       presentFeedbackFlowWithContext:kFeedbackContext];
                 }],
      [[SideMenuItem alloc]
          initWithTitle:l10n_util::GetNSString(IDS_ACTIONBAR_HELP)
                   icon:RemotingTheme.helpIcon
                 action:^{
                   [AppDelegate.instance presentHelpCenter];
                 }],
    ] ];
  });
  return items;
}

@end
