// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO (aboone) This include is for The Google Toolbox for Mac OAuth 2
// Controllers https://code.google.com/p/gtm-oauth2/ This may need to be added
// as a third-party or locate the proper project in Chromium.
#import "GTMOAuth2ViewControllerTouch.h"

#include "google_apis/google_api_keys.h"
#import "remoting/ios/authorize.h"
#include "remoting/base/service_urls.h"
// TODO (aboone) Pulling in some service values from the host side.  The cc's
// are also compiled as part of this project because the target remoting_host
// does not build on iOS right now.
#include "remoting/host/setup/oauth_helper.h"

namespace {
static NSString* const kKeychainItemName = @"Google Chromoting iOS";

NSString* ClientId() {
  return
      [NSString stringWithUTF8String:google_apis::GetOAuth2ClientID(
                                         google_apis::CLIENT_REMOTING).c_str()];
}

NSString* ClientSecret() {
  return
      [NSString stringWithUTF8String:google_apis::GetOAuth2ClientSecret(
                                         google_apis::CLIENT_REMOTING).c_str()];
}

NSString* Scopes() {
  return [NSString stringWithUTF8String:remoting::GetOauthScope().c_str()];
}

NSMutableString* HostURL() {
  return
      [NSMutableString stringWithUTF8String:remoting::ServiceUrls::GetInstance()
                                                ->directory_hosts_url()
                                                .c_str()];
}

NSString* APIKey() {
  return [NSString stringWithUTF8String:google_apis::GetAPIKey().c_str()];
}

}  // namespace

@implementation Authorize

+ (GTMOAuth2Authentication*)getAnyExistingAuthorization {
  // Ensure the google_apis lib has keys
  // If this check fails then google_apis was not built right
  // TODO (aboone) For now we specify the preprocessor macros for
  // GOOGLE_CLIENT_SECRET_REMOTING and GOOGLE_CLIENT_ID_REMOTING when building
  // the google_apis target.  The values may be developer specific, and should
  // be well know to the project staff.
  // See http://www.chromium.org/developers/how-tos/api-keys for more general
  // information.
  DCHECK(![ClientId() isEqualToString:@"dummytoken"]);

  return [GTMOAuth2ViewControllerTouch
      authForGoogleFromKeychainForName:kKeychainItemName
                              clientID:ClientId()
                          clientSecret:ClientSecret()];
}

+ (void)beginRequest:(GTMOAuth2Authentication*)authReq
             delegate:(id)delegate
    didFinishSelector:(SEL)sel {
  // Build request URL using API HTTP endpoint, and our api key
  NSMutableString* hostsUrl = HostURL();
  [hostsUrl appendString:@"?key="];
  [hostsUrl appendString:APIKey()];

  NSMutableURLRequest* theRequest =
      [NSMutableURLRequest requestWithURL:[NSURL URLWithString:hostsUrl]];

  // Add scopes if needed
  NSString* scope = authReq.scope;

  if ([scope rangeOfString:Scopes()].location == NSNotFound) {
    scope = [GTMOAuth2Authentication scopeWithStrings:scope, Scopes(), nil];
    authReq.scope = scope;
  }

  // Execute request async
  [authReq authorizeRequest:theRequest delegate:delegate didFinishSelector:sel];
}

+ (void)appendCredentials:(NSMutableURLRequest*)request {
  // Add credentials for service
  [request addValue:ClientId() forHTTPHeaderField:@"client_id"];
  [request addValue:ClientSecret() forHTTPHeaderField:@"client_secret"];
}

+ (UINavigationController*)createLoginController:(id)delegate
                                finishedSelector:(SEL)finishedSelector {
  [GTMOAuth2ViewControllerTouch
      removeAuthFromKeychainForName:kKeychainItemName];

  // When the sign in is complete a http redirection occurs, and the
  // user would see the output.  We do not want the user to notice this
  // transition.  Wrapping the oAuth2 Controller in a
  // UINavigationController causes the view to render as a blank/black
  // page when a http redirection occurs.
  return [[UINavigationController alloc]
      initWithRootViewController:[[GTMOAuth2ViewControllerTouch alloc]
                                        initWithScope:Scopes()
                                             clientID:ClientId()
                                         clientSecret:ClientSecret()
                                     keychainItemName:kKeychainItemName
                                             delegate:delegate
                                     finishedSelector:finishedSelector]];
}

@end
