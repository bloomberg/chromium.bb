// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_OPTION_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_OPTION_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_option_type.h"

// Button defining a consent bump option.
@interface ConsentBumpOptionButton : UIButton

// Whether this option is currently selected or not.
@property(nonatomic, assign) BOOL checked;
// The type of this option.
@property(nonatomic, assign) ConsentBumpOptionType type;

// Returns a ConsentBumpOptionButton with a |title| and |text|. |text| can be
// nil, in that case the title will take the full height of the option.
+ (instancetype)consentBumpOptionButtonWithTitle:(NSString*)title
                                            text:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_OPTION_BUTTON_H_
