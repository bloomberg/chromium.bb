// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/host_preferences.h"

#import "base/logging.h"
#import "remoting/client/ios/host_preferences_persistence.h"

namespace {
static NSString* const kHostPreferencesDataKeyHostDictionary =
    @"kHostPreferencesDataKeyHostDictionary";
static NSString* const kHostPreferencesHostIdKey = @"HostId";
static NSString* const kHostPreferencesPairIdKey = @"PairId";
static NSString* const kHostPreferencesPairSecretKey = @"PairSecret";
}  // namespace

@interface HostPreferences ()

// Load the known hosts from the Keychain.
// If no data exists, return an empty dictionary
+ (NSMutableDictionary*)loadHosts;

@end

@implementation HostPreferences

@synthesize hostId = _hostId;
@synthesize pairId = _pairId;
@synthesize pairSecret = _pairSecret;

#pragma mark - Public

- (void)saveToKeychain {
  NSMutableDictionary* hosts = [HostPreferences loadHosts];
  [hosts setObject:self forKey:_hostId];

  NSData* writeData = [NSKeyedArchiver archivedDataWithRootObject:hosts];

  NSError* keychainError =
      remoting::ios::WriteHostPreferencesToKeychain(writeData);

  DLOG_IF(ERROR, !keychainError) << "Could not write to keychain.";
}

+ (HostPreferences*)hostForId:(NSString*)hostId {
  NSMutableDictionary* hosts = [HostPreferences loadHosts];
  HostPreferences* host = hosts[hostId];
  if (!host) {
    host = [[HostPreferences alloc] init];
    host.hostId = hostId;
    host.pairId = @"";
    host.pairSecret = @"";
  }
  return host;
}

#pragma mark - Private

+ (NSMutableDictionary*)loadHosts {
  NSData* readData = remoting::ios::ReadHostPreferencesFromKeychain();
  if (readData) {
    return [NSKeyedUnarchiver unarchiveObjectWithData:readData];
  } else {
    return [[NSMutableDictionary alloc] init];
  }
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)coder {
  self = [super init];
  if (self) {
    [self setHostId:[coder decodeObjectForKey:kHostPreferencesHostIdKey]];
    [self setPairId:[coder decodeObjectForKey:kHostPreferencesPairIdKey]];
    [self
        setPairSecret:[coder decodeObjectForKey:kHostPreferencesPairSecretKey]];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:_hostId forKey:kHostPreferencesHostIdKey];
  [coder encodeObject:_pairId forKey:kHostPreferencesPairIdKey];
  [coder encodeObject:_pairSecret forKey:kHostPreferencesPairSecretKey];
}

@end
