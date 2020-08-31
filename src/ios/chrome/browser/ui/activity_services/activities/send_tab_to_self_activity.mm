// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSendTabToSelfActivityType =
    @"com.google.chrome.sendTabToSelfActivity";

const char kClickResultHistogramName[] = "SendTabToSelf.ShareMenu.ClickResult";

// TODO(crbug.com/970886): Move to a directory accessible on all platforms.
// State of the send tab to self option in the context menu.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendTabToSelfClickResult {
  kShowItem = 0,
  kClickItem = 1,
  kShowDeviceList = 2,
  kMaxValue = kShowDeviceList,
};

}  // namespace

@interface SendTabToSelfActivity ()
// The data object targeted by this activity.
@property(nonatomic, strong, readonly) ShareToData* data;
// The handler to be invoked when the activity is performed.
@property(nonatomic, weak, readonly) id<BrowserCommands> handler;

@end

@implementation SendTabToSelfActivity

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCommands>)handler {
  base::UmaHistogramEnumeration(kClickResultHistogramName,
                                SendTabToSelfClickResult::kShowItem);
  if (self = [super init]) {
    _data = data;
    _handler = handler;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kSendTabToSelfActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_send_tab_to_self"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.data.canSendTabToSelf;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self.handler showSendTabToSelfUI];
  [self activityDidFinish:YES];
}

@end
