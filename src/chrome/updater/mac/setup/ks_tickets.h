// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_KS_TICKETS_H_
#define CHROME_UPDATER_MAC_SETUP_KS_TICKETS_H_

#import <Foundation/Foundation.h>

#include "chrome/updater/update_service.h"

extern NSString* _Nonnull const kCRUTicketBrandKey;
extern NSString* _Nonnull const kCRUTicketTagKey;

@interface KSPathExistenceChecker : NSObject <NSSecureCoding>
@property(nonnull, readonly) NSString* path;
@end

@interface KSTicket : NSObject <NSSecureCoding>

@property(nonnull, nonatomic, readonly) NSString* productID;
@property(nonnull, nonatomic, readonly)
    KSPathExistenceChecker* existenceChecker;
@property(nullable, nonatomic, readonly) NSURL* serverURL;
@property(nonnull, nonatomic, readonly) NSDate* creationDate;
@property(nullable, nonatomic, readonly) NSString* serverType;
@property(nullable, nonatomic, readonly) NSString* tag;
@property(nullable, nonatomic, readonly) NSString* tagPath;
@property(nullable, nonatomic, readonly) NSString* tagKey;
@property(nullable, nonatomic, readonly) NSString* brandPath;
@property(nullable, nonatomic, readonly) NSString* brandKey;
@property(nullable, nonatomic, readonly) NSString* version;
@property(nullable, nonatomic, readonly) NSString* versionPath;
@property(nullable, nonatomic, readonly) NSString* versionKey;
@property(nullable, nonatomic, readonly) NSString* cohort;
@property(nullable, nonatomic, readonly) NSString* cohortHint;
@property(nullable, nonatomic, readonly) NSString* cohortName;
@property int32_t ticketVersion;

// Values that are sent as the attributes in the update check request.
- (nullable NSString*)determineTag;      // ap
- (nullable NSString*)determineBrand;    // brand
- (nullable NSString*)determineVersion;  // version

- (nullable instancetype)initWithAppState:
    (const updater::UpdateService::AppState&)state;

@end

// KSTicketStore holds a class method for reading an NSDictionary of NSString
// to KSTickets.
@interface KSTicketStore : NSObject

+ (nullable NSDictionary<NSString*, KSTicket*>*)readStoreWithPath:
    (nonnull NSString*)path;

@end

#endif  // CHROME_UPDATER_MAC_SETUP_KS_TICKETS_H_
