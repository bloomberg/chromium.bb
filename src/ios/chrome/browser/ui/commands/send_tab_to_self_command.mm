// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/send_tab_to_self_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SendTabToSelfCommand

@synthesize targetDeviceId = _targetDeviceId;

- (instancetype)initWithTargetDeviceId:(NSString*)targetDeviceId {
  if (self = [super init]) {
    _targetDeviceId = [targetDeviceId copy];
  }
  return self;
}

@end