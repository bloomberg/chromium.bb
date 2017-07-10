// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/side_menu_items.h"

#import "remoting/ios/app/app_delegate.h"

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
      // TODO(yuweih): Add icons when crbug.com/734054 is fixed.
      [[SideMenuItem alloc]
          initWithTitle:@"Send Feedback"
                   icon:nil
                 action:^{
                   [AppDelegate.instance
                       presentFeedbackFlowWithContext:kFeedbackContext];
                 }],
      [[SideMenuItem alloc] initWithTitle:@"Help"
                                     icon:nil
                                   action:^{
                                     NSLog(@"Tapped help");
                                   }],
    ] ];
  });
  return items;
}

@end
