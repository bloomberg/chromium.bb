// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_PERSONALIZATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_PERSONALIZATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_option_type.h"

// View controller displaying the Personalization screen.
@interface ConsentBumpPersonalizationViewController : UIViewController

// The currently selected option.
@property(nonatomic, assign, readonly) ConsentBumpOptionType selectedOption;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_PERSONALIZATION_VIEW_CONTROLLER_H_
