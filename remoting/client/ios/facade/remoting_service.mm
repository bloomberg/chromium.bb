// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/facade/remoting_service.h"

#import <Foundation/Foundation.h>

#import "base/mac/bind_objc_block.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/client/ios/facade/host_info.h"
#include "remoting/client/ios/facade/host_list_fetcher.h"

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
          [[RemotingService SharedInstance] runtime]->url_requester(),
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
          [[RemotingService SharedInstance] runtime]->url_requester(),
          /*auto_refresh=*/true));
  return oauth_tokenGetter;
}

@interface RemotingService () {
  std::unique_ptr<remoting::OAuthTokenGetter> _tokenGetter;
  UserInfo* _user;
  NSArray<HostInfo*>* _hosts;
  id<RemotingAuthenticationDelegate> _authDelegate;
  id<RemotingHostListDelegate> _hostListDelegate;
  remoting::HostListFetcher* _hostListFetcher;
}

@end

//
// RemodingService will act as the facade to the C++ layer that has not been
// implemented/integrated yet.
// TODO(nicholss): Implement/Integrate this class. At the moment it is being
// used to generate fake data to implement the UI of the app.
// Update: Half implemented now. User is still fake, but now real hosts lists.
//
@implementation RemotingService

// RemotingService is a singleton.
+ (RemotingService*)SharedInstance {
  static RemotingService* sharedInstance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    sharedInstance = [[RemotingService alloc] init];
  });
  return sharedInstance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _user = nil;
    _hosts = nil;
    _hostListFetcher = new remoting::HostListFetcher(
        remoting::ChromotingClientRuntime::GetInstance()->url_requester());
  }
  return self;
}

#pragma mark - RemotingService Implementation

// TODO(nicholss): isAuthenticated needs to just kick off a request to
// authenticate a user. and more than one controller might want to be a delegate
// for this info so need to change this to be more of the registration types.
// The remoting_service might also want to be registered for authentication
// changes and it can update it's cache as it needs.

- (void)setAuthenticationDelegate:(id<RemotingAuthenticationDelegate>)delegate {
  _authDelegate = delegate;
  if (_authDelegate) {
    [_authDelegate nowAuthenticated:[self isAuthenticated]];
  }
  if (!_user && _tokenGetter) {
    _tokenGetter->CallWithToken(base::BindBlockArc(
        ^(remoting::OAuthTokenGetter::Status status,
          const std::string& user_email, const std::string& access_token) {
          if (status == remoting::OAuthTokenGetter::Status::SUCCESS) {
            _user = [[UserInfo alloc] init];
            _user.userEmail =
                [NSString stringWithCString:user_email.c_str()
                                   encoding:[NSString defaultCStringEncoding]];
          } else {
            _user = nil;
          }
          if (_authDelegate) {
            [_authDelegate nowAuthenticated:[self isAuthenticated]];
          }
        }));
  }
}

- (BOOL)isAuthenticated {
  if (_user) {
    return YES;
  }
  return NO;
}

- (void)startHostListFetchWith:(NSString*)accessToken {
  NSLog(@"startHostListFetchWith : %@ %@", accessToken, _authDelegate);
  if (_authDelegate) {
    [_authDelegate nowAuthenticated:YES];

    _hostListFetcher->RetrieveHostlist(
        base::SysNSStringToUTF8(accessToken),
        base::BindBlockArc(^(const std::vector<remoting::HostInfo>& hostlist) {
          NSMutableArray<HostInfo*>* hosts =
              [NSMutableArray arrayWithCapacity:hostlist.size()];
          std::string status;
          for (const remoting::HostInfo& host_info : hostlist) {
            remoting::HostStatus host_status = host_info.status;
            switch (host_status) {
              case remoting::kHostStatusOnline:
                status = "ONLINE";
                break;
              case remoting::kHostStatusOffline:
                status = "OFFLINE";
                break;
              default:
                NOTREACHED();
            }
            // TODO(nicholss): Not yet integrated: createdTime, hostVersion,
            // kind, offlineReason. Add them as the app will need this info.
            HostInfo* host = [[HostInfo alloc] init];
            host.hostId =
                [NSString stringWithCString:host_info.host_id.c_str()
                                   encoding:[NSString defaultCStringEncoding]];
            host.hostName =
                [NSString stringWithCString:host_info.host_name.c_str()
                                   encoding:[NSString defaultCStringEncoding]];
            host.jabberId =
                [NSString stringWithCString:host_info.host_jid.c_str()
                                   encoding:[NSString defaultCStringEncoding]];
            host.publicKey =
                [NSString stringWithCString:host_info.public_key.c_str()
                                   encoding:[NSString defaultCStringEncoding]];
            host.status =
                [NSString stringWithCString:status.c_str()
                                   encoding:[NSString defaultCStringEncoding]];
            [hosts addObject:host];
          }
          _hosts = hosts;
          [_hostListDelegate hostListUpdated];
        }));
  }
}

- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode {
  _tokenGetter = CreateOAuthTokenGetterWithAuthorizationCode(
      std::string(base::SysNSStringToUTF8(authorizationCode)),
      base::BindBlockArc(
          ^(const std::string& user_email, const std::string& refresh_token) {
            // TODO(nicholss): Do something with these new creds.
            VLOG(1) << "New Creds: " << user_email << " " << refresh_token;
          }));
}

- (void)authenticateWithRefreshToken:(NSString*)refreshToken
                               email:(NSString*)email {
  _tokenGetter = CreateOAuthTokenWithRefreshToken(
      std::string(base::SysNSStringToUTF8(refreshToken)),
      base::SysNSStringToUTF8(email));
}

- (UserInfo*)getUser {
  if (![self isAuthenticated]) {
    return nil;
  }

  NSMutableString* json = [[NSMutableString alloc] init];
  [json appendString:@"{"];
  [json appendString:@"\"userId\":\"AABBCC123\","];
  [json appendString:@"\"userFullName\":\"John Smith\","];
  [json appendString:@"\"userEmail\":\"john@example.com\","];
  [json appendString:@"}"];

  NSMutableData* data = [NSMutableData
      dataWithData:[[json copy] dataUsingEncoding:NSUTF8StringEncoding]];

  UserInfo* user = [UserInfo parseListFromJSON:data];
  return user;
}

- (void)setHostListDelegate:(id<RemotingHostListDelegate>)delegate {
  bool attemptUpdate = (_hostListDelegate != delegate);
  _hostListDelegate = delegate;
  if (attemptUpdate && _hostListDelegate && _tokenGetter) {
    // TODO(nicholss): It might be cleaner to set the delegate and then have
    // them ask to refresh the host list rather than start this get hosts call.
    _tokenGetter->CallWithToken(base::BindBlockArc(
        ^(remoting::OAuthTokenGetter::Status status,
          const std::string& user_email, const std::string& access_token) {
          NSString* accessToken =
              [NSString stringWithCString:access_token.c_str()
                                 encoding:[NSString defaultCStringEncoding]];
          [self startHostListFetchWith:accessToken];
        }));
  }
}

- (NSArray<HostInfo*>*)getHosts {
  if (![self isAuthenticated]) {
    return nil;
  }
  return _hosts;
}

- (remoting::ChromotingClientRuntime*)runtime {
  return remoting::ChromotingClientRuntime::GetInstance();
}

@end
