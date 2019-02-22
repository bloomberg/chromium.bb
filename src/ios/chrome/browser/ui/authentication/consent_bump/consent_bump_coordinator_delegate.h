// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ConsentBumpCoordinator;

// Protocol defining a delegate for the ConsentBump coordinator.
@protocol ConsentBumpCoordinatorDelegate

// Notifies the delegate that the |coordinator| has finished with the type
// |optionType|.
- (void)consentBumpCoordinator:(ConsentBumpCoordinator*)coordinator
    didFinishNeedingToShowSettings:(BOOL)shouldShowSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_COORDINATOR_DELEGATE_H_
