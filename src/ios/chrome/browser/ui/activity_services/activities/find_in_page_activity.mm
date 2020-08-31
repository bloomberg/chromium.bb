// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kFindInPageActivityType =
    @"com.google.chrome.FindInPageActivityType";

}  // namespace

@interface FindInPageActivity ()
// Data associated with this activity.
@property(nonatomic, strong, readonly) ShareToData* data;
// The handler that handles the command when the activity is performed.
@property(nonatomic, weak, readonly) id<FindInPageCommands> handler;
@end

@implementation FindInPageActivity

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<FindInPageCommands>)handler {
  self = [super init];
  if (self) {
    _data = data;
    _handler = handler;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kFindInPageActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_FIND_IN_PAGE);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_find_in_page"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.data.isPageSearchable;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self.handler openFindInPage];
  [self activityDidFinish:YES];
}

@end
