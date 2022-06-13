// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace first_run {

// The accessibility identifier for the UMA collection checkbox shown in first
// run.
extern NSString* const kUMAMetricsButtonAccessibilityIdentifier;

// The accessibility identifier for the Welcome screen shown in first run.
extern NSString* const kFirstRunWelcomeScreenAccessibilityIdentifier;

// The accessibility identifier for the Sign in screen shown in first run.
extern NSString* const kFirstRunSignInScreenAccessibilityIdentifier;

// The accessibility identifier for the Sync screen shown in first run.
extern NSString* const kFirstRunSyncScreenAccessibilityIdentifier;

// The accessibility identifier for the Default browser screen shown in first
// run.
extern NSString* const kFirstRunDefaultBrowserScreenAccessibilityIdentifier;

}  // first_run

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_
