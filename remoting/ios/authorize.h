// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_AUTHORIZE_H_
#define REMOTING_IOS_AUTHORIZE_H_

#import <UIKit/UIKit.h>

// TODO (aboone) This include is for The Google Toolbox for Mac OAuth 2
// https://code.google.com/p/gtm-oauth2/ This may need to be added as a
// third-party or locate the proper project in Chromium.
#import "GTMOAuth2Authentication.h"

@interface Authorize : NSObject

+ (GTMOAuth2Authentication*)getAnyExistingAuthorization;

+ (void)beginRequest:(GTMOAuth2Authentication*)authorization
             delegate:self
    didFinishSelector:(SEL)sel;

+ (void)appendCredentials:(NSMutableURLRequest*)request;

+ (UINavigationController*)createLoginController:(id)delegate
                                finishedSelector:(SEL)finishedSelector;

@end

#endif  // REMOTING_IOS_AUTHORIZE_H_
