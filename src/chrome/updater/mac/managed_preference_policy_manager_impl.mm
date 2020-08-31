// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/managed_preference_policy_manager_impl.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/updater/constants.h"

// Constants for managed preference policy keys.
static NSString* kGlobalPolicyKey = @"global";
static NSString* kUpdateDefaultKey = @"UpdateDefault";
static NSString* kDownloadPreferenceKey = @"DownloadPreference";
static NSString* kUpdatesSuppressedStartHourKey = @"UpdatesSuppressedStartHour";
static NSString* kUpdatesSuppressedStartMinuteKey =
    @"UpdatesSuppressedStartMin";
static NSString* kUpdatesSuppressedDurationMinuteKey =
    @"UpdatesSuppressedDurationMin";
static NSString* kTargetVersionPrefixKey = @"TargetVersionPrefix";
static NSString* kRollbackToTargetVersionKey = @"RollbackToTargetVersion";

namespace updater {

namespace {

// Extracts an integer value from a NSString or NSNumber. Returns kPolicyNotSet
// for all unexpected cases.
int ReadPolicyInteger(id value) {
  if (!value)
    return kPolicyNotSet;

  NSInteger result = kPolicyNotSet;
  if ([value isKindOfClass:[NSString class]]) {
    NSScanner* scanner = [NSScanner scannerWithString:(NSString*)value];
    if (![scanner scanInteger:&result]) {
      return kPolicyNotSet;
    }
  } else if ([value isKindOfClass:[NSNumber class]]) {
    result = [(NSNumber*)value intValue];
  }

  return static_cast<int>(result);
}

// Reads a policy NSString value. Returns nil for all unexpected cases.
base::scoped_nsobject<NSString> ReadPolicyString(id value) {
  if ([value isKindOfClass:[NSString class]])
    return base::scoped_nsobject<NSString>([value copy]);
  else
    return base::scoped_nsobject<NSString>(nil);
}

}  // namespace

}  // namespace updater

/// Class that manages Mac global-level policies.
@interface CRUManagedPreferenceGlobalPolicySettings : NSObject {
  base::scoped_nsobject<NSString> _downloadPreference;
};

@property(nonatomic, readonly) int lastCheckPeriodMinutes;
@property(nonatomic, readonly) int defaultUpdatePolicy;
@property(nonatomic, readonly, nullable) NSString* downloadPreference;
@property(nonatomic, readonly, nullable) NSString* proxyMode;
@property(nonatomic, readonly, nullable) NSString* proxyServer;
@property(nonatomic, readonly, nullable) NSString* proxyPacURL;
@property(nonatomic, readonly) CRUUpdatesSuppressed updatesSuppressed;

@end

@implementation CRUManagedPreferenceGlobalPolicySettings

@synthesize defaultUpdatePolicy = _defaultUpdatePolicy;
@synthesize updatesSuppressed = _updatesSuppressed;

- (instancetype)initWithDictionary:(CRUAppPolicyDictionary*)policyDict {
  if (([super init])) {
    _downloadPreference = updater::ReadPolicyString(
        [policyDict objectForKey:kDownloadPreferenceKey]);
    _defaultUpdatePolicy =
        updater::ReadPolicyInteger(policyDict[kUpdateDefaultKey]);
    _updatesSuppressed.start_hour =
        updater::ReadPolicyInteger(policyDict[kUpdatesSuppressedStartHourKey]);
    _updatesSuppressed.start_minute = updater::ReadPolicyInteger(
        policyDict[kUpdatesSuppressedStartMinuteKey]);
    _updatesSuppressed.duration_minute = updater::ReadPolicyInteger(
        policyDict[kUpdatesSuppressedDurationMinuteKey]);
  }
  return self;
}

- (int)lastCheckPeriodMinutes {
  // LastCheckPeriodMinutes is not supported in Managed Preference policy.
  return kPolicyNotSet;
}

- (NSString*)downloadPreference {
  if (_downloadPreference) {
    return [NSString stringWithString:_downloadPreference];
  } else {
    return nil;
  }
}

- (NSString*)proxyMode {
  return nil;  //  ProxyMode is not supported in Managed Preference policy.
}

- (NSString*)proxyServer {
  return nil;  // ProxyServer is not supported in Managed Preference policy.
}

- (NSString*)proxyPacURL {
  return nil;  //  ProxyPacURL is not supported in Managed Preference policy.
}

@end

/// Class that manages policies for a single App.
@interface CRUManagedPreferenceAppPolicySettings : NSObject {
  base::scoped_nsobject<NSString> _targetVersionPrefix;
}

@property(nonatomic, readonly) int updatePolicy;
@property(nonatomic, readonly) int rollbackToTargetVersion;
@property(nonatomic, readonly, nullable) NSString* targetVersionPrefix;

@end

@implementation CRUManagedPreferenceAppPolicySettings

@synthesize updatePolicy = _updatePolicy;
@synthesize rollbackToTargetVersion = _rollbackToTargetVersion;

- (instancetype)initWithDictionary:(CRUAppPolicyDictionary*)policyDict {
  if (([super init])) {
    _updatePolicy =
        updater::ReadPolicyInteger([policyDict objectForKey:kUpdateDefaultKey]);
    _targetVersionPrefix = updater::ReadPolicyString(
        [policyDict objectForKey:kTargetVersionPrefixKey]);
    _rollbackToTargetVersion = updater::ReadPolicyInteger(
        [policyDict objectForKey:kRollbackToTargetVersionKey]);
  }

  return self;
}

- (NSString*)targetVersionPrefix {
  if (_targetVersionPrefix) {
    return [NSString stringWithString:_targetVersionPrefix];
  } else {
    return nil;
  }
}

@end

@implementation CRUManagedPreferencePolicyManager {
  base::scoped_nsobject<CRUManagedPreferenceGlobalPolicySettings> _globalPolicy;
  base::scoped_nsobject<
      NSMutableDictionary<NSString*, CRUManagedPreferenceAppPolicySettings*>>
      _appPolicies;
}

@synthesize managed = _managed;

- (instancetype)initWithDictionary:(CRUUpdatePolicyDictionary*)policies {
  if (([super init])) {
    _managed = (policies.count > 0);

    // Always create a global policy instance for default values.
    _globalPolicy.reset([[CRUManagedPreferenceGlobalPolicySettings alloc]
        initWithDictionary:nil]);

    _appPolicies.reset([[NSMutableDictionary alloc] init]);
    for (NSString* appid in policies.allKeys) {
      if (![policies[appid] isKindOfClass:[CRUAppPolicyDictionary class]])
        continue;

      CRUAppPolicyDictionary* policyDict = policies[appid];
      appid = appid.lowercaseString;
      if ([appid isEqualToString:kGlobalPolicyKey]) {
        _globalPolicy.reset([[CRUManagedPreferenceGlobalPolicySettings alloc]
            initWithDictionary:policyDict]);
      } else {
        base::scoped_nsobject<CRUManagedPreferenceAppPolicySettings>
            appSettings([[CRUManagedPreferenceAppPolicySettings alloc]
                initWithDictionary:policyDict]);
        [_appPolicies setObject:appSettings.get() forKey:appid];
      }
    }
  }
  return self;
}

- (NSString*)source {
  return @"ManagedPreference";
}

- (NSString*)downloadPreference {
  return [_globalPolicy downloadPreference];
}

- (NSString*)proxyMode {
  return [_globalPolicy proxyMode];
}

- (NSString*)proxyServer {
  return [_globalPolicy proxyServer];
}

- (NSString*)proxyPacURL {
  return [_globalPolicy proxyPacURL];
}

- (int)lastCheckPeriodMinutes {
  return [_globalPolicy lastCheckPeriodMinutes];
}

- (int)defaultUpdatePolicy {
  return [_globalPolicy defaultUpdatePolicy];
}

- (CRUUpdatesSuppressed)updatesSuppressed {
  return [_globalPolicy updatesSuppressed];
}

- (int)appUpdatePolicy:(NSString*)appid {
  appid = appid.lowercaseString;
  if (![_appPolicies objectForKey:appid])
    return kPolicyNotSet;
  return [_appPolicies objectForKey:appid].updatePolicy;
}

- (NSString*)targetVersionPrefix:(NSString*)appid {
  appid = appid.lowercaseString;
  return [_appPolicies objectForKey:appid].targetVersionPrefix;
}

- (int)rollbackToTargetVersion:(NSString*)appid {
  appid = appid.lowercaseString;
  if (![_appPolicies objectForKey:appid])
    return kPolicyNotSet;
  return [_appPolicies objectForKey:appid].rollbackToTargetVersion;
}

@end
