// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channel_item.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowedWebChannelItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [FollowedWebChannelCell class];
  }
  return self;
}

#pragma mark - Properties

- (NSString*)title {
  return _followedWebChannel.title;
}

- (CrURL*)URL {
  return (_followedWebChannel.rssURL.nsurl) ? _followedWebChannel.rssURL
                                            : _followedWebChannel.webPageURL;
}

- (NSString*)thirdRowText {
  if (!_followedWebChannel.available) {
    return l10n_util::GetNSString(
        IDS_IOS_FOLLOW_MANAGEMENT_CHANNEL_UNAVAILABLE);
  }
  return nil;
}

- (UIColor*)thirdRowTextColor {
  // TODO(crbug.com/1296745): Polish color.
  return [UIColor redColor];
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  FollowedWebChannelCell* cell =
      base::mac::ObjCCastStrict<FollowedWebChannelCell>(tableCell);
  cell.followedWebChannel = self.followedWebChannel;
}

@end

@implementation FollowedWebChannelCell
@end
