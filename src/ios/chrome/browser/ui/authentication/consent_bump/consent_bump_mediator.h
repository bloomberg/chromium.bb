// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ConsentBumpConsumer;

// Type of consent bump sub-screen.
typedef NS_ENUM(NSInteger, ConsentBumpScreen) {
  ConsentBumpScreenUnifiedConsent,
  ConsentBumpScreenPersonalization,
};

// Mediator for the consent bump, updating the consumer as needed.
@interface ConsentBumpMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<ConsentBumpConsumer> consumer;

// Updates the consumer such as it is displaying information relative to the
// |consentBumpScreen|.
- (void)updateConsumerForConsentBumpScreen:(ConsentBumpScreen)consentBumpScreen;
// Updates the consumer shuch as it is possible to continue on the next screen
// by showing the primary button.
- (void)consumerCanProceed;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_MEDIATOR_H_
