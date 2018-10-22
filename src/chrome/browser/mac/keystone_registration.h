// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_KEYSTONE_REGISTRATION_H_
#define CHROME_BROWSER_MAC_KEYSTONE_REGISTRATION_H_

#import <Foundation/Foundation.h>
#include <Security/Authorization.h>
#include <stdint.h>

// Declarations of the Keystone registration bits needed here. From
// KSRegistration.h.

namespace keystone_registration {

typedef enum {
  kKSPathExistenceChecker,
} KSExistenceCheckerType;

typedef enum {
  kKSRegistrationUserTicket,
  kKSRegistrationSystemTicket,
  kKSRegistrationDontKnowWhatKindOfTicket,
} KSRegistrationTicketType;

extern NSString* KSRegistrationVersionKey;
extern NSString* KSRegistrationExistenceCheckerTypeKey;
extern NSString* KSRegistrationExistenceCheckerStringKey;
extern NSString* KSRegistrationServerURLStringKey;
extern NSString* KSRegistrationPreserveTrustedTesterTokenKey;
extern NSString* KSRegistrationTagKey;
extern NSString* KSRegistrationTagPathKey;
extern NSString* KSRegistrationTagKeyKey;
extern NSString* KSRegistrationBrandPathKey;
extern NSString* KSRegistrationBrandKeyKey;
extern NSString* KSRegistrationVersionPathKey;
extern NSString* KSRegistrationVersionKeyKey;

extern NSString* KSRegistrationDidCompleteNotification;
extern NSString* KSRegistrationPromotionDidCompleteNotification;

extern NSString* KSRegistrationCheckForUpdateNotification;
extern NSString* KSRegistrationStatusKey;
extern NSString* KSRegistrationUpdateCheckErrorKey;
extern NSString* KSRegistrationUpdateCheckRawResultsKey;
extern NSString* KSRegistrationUpdateCheckRawErrorMessagesKey;

extern NSString* KSRegistrationStartUpdateNotification;
extern NSString* KSUpdateCheckSuccessfulKey;
extern NSString* KSUpdateCheckSuccessfullyInstalledKey;

extern NSString* KSRegistrationRemoveExistingTag;

extern NSString* KSReportingAttributeValueKey;
extern NSString* KSReportingAttributeExpirationDateKey;
extern NSString* KSReportingAttributeAggregationTypeKey;
#define KSRegistrationPreserveExistingTag nil

}  // namespace keystone_registration

typedef enum {
  kKSReportingAggregationSum = 0,  // Adds attribute value across user accounts
  kKSReportingAggregationDefault = kKSReportingAggregationSum,
} KSReportingAggregationType;

@interface KSRegistration : NSObject

+ (id)registrationWithProductID:(NSString*)productID;

- (BOOL)registerWithParameters:(NSDictionary*)args;

- (BOOL)promoteWithParameters:(NSDictionary*)args
                authorization:(AuthorizationRef)authorization;

- (BOOL)setActive;
- (BOOL)setActiveWithReportingAttributes:(NSArray*)reportingAttributes
                                   error:(NSError**)error;
- (void)checkForUpdateWasUserInitiated:(BOOL)userInitiated;
- (void)startUpdate;
- (keystone_registration::KSRegistrationTicketType)ticketType;

@end  // @interface KSRegistration


// Declarations of the Keystone attribute reporting bits needed here.
// Full definition is at:
// //depot/googlemac/opensource/update-engine/Common/KSReportingAttribute.h
@interface KSReportingAttribute : NSObject

@end  // @interface KSReportingAttribute

@interface KSUnsignedReportingAttribute : KSReportingAttribute

+ (KSUnsignedReportingAttribute *)reportingAttributeWithValue:(uint32_t)value
               name:(NSString *)name
    aggregationType:(KSReportingAggregationType)aggregationType
              error:(NSError **)error;

@end  // @interface KSUnsignedReportingAttribute


#endif  // CHROME_BROWSER_MAC_KEYSTONE_REGISTRATION_H_
