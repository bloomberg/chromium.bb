// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/facade/remoting_oauth_authentication.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "base/mac/bind_objc_block.h"
#import "remoting/ios/facade/host_info.h"
#import "remoting/ios/facade/host_list_fetcher.h"
#import "remoting/ios/facade/ios_client_runtime_delegate.h"
#import "remoting/ios/facade/remoting_service.h"
#import "remoting/ios/keychain_wrapper.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"

static NSString* const kCRDAuthenticatedUserEmailKey =
    @"kCRDAuthenticatedUserEmailKey";

const char kOauthRedirectUrl[] =
    "https://chromoting-oauth.talkgadget."
    "google.com/talkgadget/oauth/chrome-remote-desktop/dev";

std::unique_ptr<remoting::OAuthTokenGetter>
CreateOAuthTokenGetterWithAuthorizationCode(
    const std::string& auth_code,
    const remoting::OAuthTokenGetter::CredentialsUpdatedCallback&
        on_credentials_update) {
  std::unique_ptr<remoting::OAuthTokenGetter::OAuthIntermediateCredentials>
      oauth_credentials(
          new remoting::OAuthTokenGetter::OAuthIntermediateCredentials(
              auth_code, /*is_service_account=*/false));
  oauth_credentials->oauth_redirect_uri = kOauthRedirectUrl;

  std::unique_ptr<remoting::OAuthTokenGetter> oauth_tokenGetter(
      new remoting::OAuthTokenGetterImpl(
          std::move(oauth_credentials), on_credentials_update,
          RemotingService.instance.runtime->url_requester(),
          /*auto_refresh=*/true));
  return oauth_tokenGetter;
}

std::unique_ptr<remoting::OAuthTokenGetter> CreateOAuthTokenWithRefreshToken(
    const std::string& refresh_token,
    const std::string& email) {
  std::unique_ptr<remoting::OAuthTokenGetter::OAuthAuthorizationCredentials>
      oauth_credentials(
          new remoting::OAuthTokenGetter::OAuthAuthorizationCredentials(
              email, refresh_token, /*is_service_account=*/false));

  std::unique_ptr<remoting::OAuthTokenGetter> oauth_tokenGetter(
      new remoting::OAuthTokenGetterImpl(
          std::move(oauth_credentials),
          RemotingService.instance.runtime->url_requester(),
          /*auto_refresh=*/true));
  return oauth_tokenGetter;
}

RemotingAuthenticationStatus oauthStatusToRemotingAuthenticationStatus(
    remoting::OAuthTokenGetter::Status status) {
  switch (status) {
    case remoting::OAuthTokenGetter::Status::AUTH_ERROR:
      return RemotingAuthenticationStatusAuthError;
    case remoting::OAuthTokenGetter::Status::NETWORK_ERROR:
      return RemotingAuthenticationStatusNetworkError;
    case remoting::OAuthTokenGetter::Status::SUCCESS:
      return RemotingAuthenticationStatusSuccess;
  }
}

@interface RemotingOAuthAuthentication () {
  std::unique_ptr<remoting::OAuthTokenGetter> _tokenGetter;
  KeychainWrapper* _keychainWrapper;
  BOOL _firstLoadUserAttempt;
}
@end

@implementation RemotingOAuthAuthentication

@synthesize user = _user;
@synthesize delegate = _delegate;

- (instancetype)init {
  self = [super init];
  if (self) {
    _keychainWrapper = [[KeychainWrapper alloc] init];
    _user = nil;
    _firstLoadUserAttempt = YES;
  }
  return self;
}

#pragma mark - Property Overrides

- (UserInfo*)user {
  if (_firstLoadUserAttempt && _user == nil) {
    _firstLoadUserAttempt = NO;
    [self setUser:[self loadUserInfo]];
  }
  return _user;
}

- (void)setUser:(UserInfo*)user {
  _user = user;
  [self storeUserInfo:_user];
  [_delegate userDidUpdate:_user];
}

#pragma mark - Class Implementation

- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode {
  __weak RemotingOAuthAuthentication* weakSelf = self;
  _tokenGetter = CreateOAuthTokenGetterWithAuthorizationCode(
      std::string(base::SysNSStringToUTF8(authorizationCode)),
      base::BindBlockArc(
          ^(const std::string& user_email, const std::string& refresh_token) {
            // TODO(nicholss): Do something with these new creds.
            VLOG(1) << "New Creds: " << user_email << " " << refresh_token;
            UserInfo* user = [[UserInfo alloc] init];
            user.userEmail = base::SysUTF8ToNSString(user_email);
            user.refreshToken = base::SysUTF8ToNSString(refresh_token);
            [weakSelf setUser:user];
          }));
  // Stimulate the oAuth Token Getter to fetch and access token, this forces it
  // to convert the authorization code into a refresh token, and saving the
  // refresh token will happen automaticly in the above block.
  [self callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                  NSString* user_email,
                                  NSString* access_token) {
    if (status == RemotingAuthenticationStatusSuccess) {
      VLOG(1) << "Success fetching access token from authorization code.";
    } else {
      LOG(ERROR) << "Failed to fetch access token from authorization code. ("
                 << status << ")";
      // TODO(nicholss): Deal with the sad path for a bad auth token.
    }
  }];
}

#pragma mark - Private

// Provide the |refreshToken| and |email| to authenticate a user as a returning
// user of the application.
- (void)authenticateWithRefreshToken:(NSString*)refreshToken
                               email:(NSString*)email {
  _tokenGetter = CreateOAuthTokenWithRefreshToken(
      std::string(base::SysNSStringToUTF8(refreshToken)),
      base::SysNSStringToUTF8(email));
}

- (void)callbackWithAccessToken:(AccessTokenCallback)onAccessToken {
  // TODO(nicholss): Be careful here since a failure to reset onAccessToken
  // will end up with retain cycle and memory leakage.
  if (_tokenGetter) {
    _tokenGetter->CallWithToken(base::BindBlockArc(
        ^(remoting::OAuthTokenGetter::Status status,
          const std::string& user_email, const std::string& access_token) {
          onAccessToken(oauthStatusToRemotingAuthenticationStatus(status),
                        base::SysUTF8ToNSString(user_email),
                        base::SysUTF8ToNSString(access_token));
        }));
  }
}

- (void)logout {
  [self storeUserInfo:nil];
  [self setUser:nil];
}

#pragma mark - Persistence

- (void)storeUserInfo:(UserInfo*)user {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if (user) {
    [defaults setObject:user.userEmail forKey:kCRDAuthenticatedUserEmailKey];
    // TODO(nicholss): Need to match the token with the email.
    [_keychainWrapper setRefreshToken:user.refreshToken];
  } else {
    [defaults removeObjectForKey:kCRDAuthenticatedUserEmailKey];
    [_keychainWrapper resetKeychainItem];
  }
  [defaults synchronize];
}

- (UserInfo*)loadUserInfo {
  UserInfo* user = [[UserInfo alloc] init];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  user.userEmail = [defaults objectForKey:kCRDAuthenticatedUserEmailKey];
  // TODO(nicholss): Need to match the token with the email.
  user.refreshToken = [_keychainWrapper refreshToken];

  if (!user || ![user isAuthenticated]) {
    user = nil;
  } else {
    [self authenticateWithRefreshToken:user.refreshToken email:user.userEmail];
  }
  return user;
}

@end
