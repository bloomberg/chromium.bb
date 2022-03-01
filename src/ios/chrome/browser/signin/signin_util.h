// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#import <UIKit/UIKit.h>

#include <set>
#include <string>

#include "ios/chrome/browser/signin/constants.h"

@class ChromeIdentity;

namespace signin {
enum class Tribool;
}  // namespace signin

// Returns an NSArray of |scopes| as NSStrings.
NSArray* GetScopeArray(const std::set<std::string>& scopes);

// Returns whether the given signin |error| should be handled.
//
// Note that cancel errors and errors handled internally by the signin component
// should not be handled.
bool ShouldHandleSigninError(NSError* error);

// Returns CGSize based on |IdentityAvatarSize|.
CGSize GetSizeForIdentityAvatarSize(IdentityAvatarSize avatar_size);

// Returns whether Chrome has been started after a device restore. This method
// needs to be called for the first time before IO is disallowed on UI thread.
// The value is cached. The result is cached for later calls.
signin::Tribool IsFirstSessionAfterDeviceRestore();

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
