// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_

#import <Foundation/Foundation.h>

#include "ios/web/common/user_agent.h"

@class CRWNavigationItemStorage;
@class CRWSessionUserData;
@class CRWSessionCertificatePolicyCacheStorage;

// NSCoding-compliant class used to serialize session state.
// TODO(crbug.com/685388): Investigate using code from the sessions component.
@interface CRWSessionStorage : NSObject <NSCoding>

@property(nonatomic, assign) BOOL hasOpener;
@property(nonatomic, assign) NSInteger lastCommittedItemIndex;
@property(nonatomic, copy) NSArray<CRWNavigationItemStorage*>* itemStorages;
@property(nonatomic, strong)
    CRWSessionCertificatePolicyCacheStorage* certPolicyCacheStorage;
@property(nonatomic, strong) CRWSessionUserData* userData;
@property(nonatomic, assign) web::UserAgentType userAgentType;
@property(nonatomic, copy) NSString* stableIdentifier;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_
