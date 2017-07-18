// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/persistence/remoting_preferences.h"

#import "base/mac/bind_objc_block.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/domain/host_settings.h"

#include "base/logging.h"

static NSString* const kActiveUserKey = @"kActiveUserKey";
static NSString* const kHostSettingsKey = @"kHostSettingsKey";

@interface RemotingPreferences () {
  NSUserDefaults* _defaults;
}
@end

@implementation RemotingPreferences

// RemotingService is a singleton.
+ (RemotingPreferences*)instance {
  static RemotingPreferences* sharedInstance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    sharedInstance = [[RemotingPreferences alloc] init];
  });
  return sharedInstance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _defaults = [NSUserDefaults standardUserDefaults];
  }
  return self;
}

#pragma mark - RemotingPreferences Implementation

- (HostSettings*)settingsForHost:(NSString*)hostId {
  NSString* key =
      [NSString stringWithFormat:@"%@-%@", kHostSettingsKey, hostId];
  NSData* encodedSettings = [_defaults objectForKey:key];
  HostSettings* settings =
      [NSKeyedUnarchiver unarchiveObjectWithData:encodedSettings];
  if (settings == nil) {
    settings = [[HostSettings alloc] init];
    settings.hostId = hostId;
    [self setSettings:settings forHost:hostId];
  }
  return settings;
}

- (void)setSettings:(HostSettings*)settings forHost:(NSString*)hostId {
  NSString* key =
      [NSString stringWithFormat:@"%@-%@", kHostSettingsKey, hostId];
  if (settings) {
    NSData* encodedSettings =
        [NSKeyedArchiver archivedDataWithRootObject:settings];
    [_defaults setObject:encodedSettings forKey:key];
  } else {
    return [_defaults removeObjectForKey:key];
  }
  [_defaults synchronize];
}

#pragma mark - Properties

- (void)setActiveUserKey:(NSString*)activeUserKey {
  if (activeUserKey) {
    [_defaults setObject:activeUserKey forKey:kActiveUserKey];
  } else {
    [_defaults removeObjectForKey:kActiveUserKey];
  }
  [_defaults synchronize];
}

- (NSString*)activeUserKey {
  return [_defaults objectForKey:kActiveUserKey];
}

@end
