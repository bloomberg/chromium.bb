// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DATA_SOURCE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVIdentity;

// Data source of CWVSyncController.
@protocol CWVSyncControllerDataSource<NSObject>

// Called when access tokens are requested.
// |identity| The user whose access tokens are requested.
// |scopes| OAuth scopes requested.
// |completionHandler| Use to pass back token information.
// If successful, only |accessToken| and |expirationDate| will be non-nil.
// If unsuccessful, only |error| will be non-nil.
- (void)fetchAccessTokenForIdentity:(CWVIdentity*)identity
                             scopes:(NSArray<NSString*>*)scopes
                  completionHandler:
                      (void (^)(NSString* _Nullable accessToken,
                                NSDate* _Nullable expirationDate,
                                NSError* _Nullable error))completionHandler;

// Return all available identities. This is used internally to track if accounts
// become stale and need to be removed.
- (NSArray<CWVIdentity*>*)allKnownIdentities;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DATA_SOURCE_H_
