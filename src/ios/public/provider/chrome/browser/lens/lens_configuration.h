// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@class ChromeIdentity;

// Configuration object used by the LensProvider.
@interface LensConfiguration : NSObject

// The current identity associated with the browser.
@property(nonatomic, assign) ChromeIdentity* identity;

// Whether or not the browser is currently in incognito mode.
@property(nonatomic, assign) BOOL isIncognito;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_CONFIGURATION_H_
