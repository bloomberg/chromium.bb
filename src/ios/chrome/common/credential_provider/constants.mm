// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/constants.h"

#include <ostream>

#include "base/check.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using app_group::ApplicationGroup;

namespace {

// Filename for the archivable storage.
NSString* const kArchivableStorageFilename = @"credential_store";

// Credential Provider dedicated shared folder name.
NSString* const kCredentialProviderContainer = @"credential_provider";

// Used to generate the key for the app group user defaults containing the
// managed user ID to be validated in the extension.
NSString* const kUserDefaultsCredentialProviderManagedUserID =
    @"kUserDefaultsCredentialProviderManagedUserID";

// Used to generate a unique AppGroupPrefix to differentiate between different
// versions of Chrome running in the same device.
NSString* AppGroupPrefix() {
  NSDictionary* infoDictionary = [NSBundle mainBundle].infoDictionary;
  NSString* prefix = infoDictionary[@"MainAppBundleID"];
  if (prefix) {
    return prefix;
  }
  return [NSBundle mainBundle].bundleIdentifier;
}

}  // namespace

NSURL* CredentialProviderSharedArchivableStoreURL() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* credentialProviderURL =
      [groupURL URLByAppendingPathComponent:kCredentialProviderContainer];
  NSString* filename =
      [AppGroupPrefix() stringByAppendingString:kArchivableStorageFilename];
  return [credentialProviderURL URLByAppendingPathComponent:filename];
}

NSString* AppGroupUserDefaultsCredentialProviderManagedUserID() {
  return [AppGroupPrefix()
      stringByAppendingString:kUserDefaultsCredentialProviderManagedUserID];
}

NSArray<NSString*>* UnusedUserDefaultsCredentialProviderKeys() {
  return @[
    @"UserDefaultsCredentialProviderASIdentityStoreSyncCompleted.V0",
    @"UserDefaultsCredentialProviderFirstTimeSyncCompleted.V0"
  ];
}

NSString* const kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted =
    @"UserDefaultsCredentialProviderASIdentityStoreSyncCompleted.V1";

NSString* const kUserDefaultsCredentialProviderFirstTimeSyncCompleted =
    @"UserDefaultsCredentialProviderFirstTimeSyncCompleted.V1";

NSString* const kUserDefaultsCredentialProviderConsentVerified =
    @"UserDefaultsCredentialProviderConsentVerified";
