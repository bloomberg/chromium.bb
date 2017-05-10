// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/facade/remoting_service.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "base/mac/bind_objc_block.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/domain/user_info.h"
#import "remoting/ios/facade/host_info.h"
#import "remoting/ios/facade/host_list_fetcher.h"
#import "remoting/ios/facade/ios_client_runtime_delegate.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#import "remoting/ios/keychain_wrapper.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"

static NSString* const kCRDAuthenticatedUserEmailKey =
    @"kCRDAuthenticatedUserEmailKey";

NSString* const kHostsDidUpdate = @"kHostsDidUpdate";

NSString* const kUserDidUpdate = @"kUserDidUpdate";
NSString* const kUserInfo = @"kUserInfo";

@interface RemotingService ()<RemotingAuthenticationDelegate> {
  std::unique_ptr<remoting::OAuthTokenGetter> _tokenGetter;
  remoting::HostListFetcher* _hostListFetcher;
  remoting::IosClientRuntimeDelegate* _clientRuntimeDelegate;
}
@end

@implementation RemotingService

@synthesize authentication = _authentication;
@synthesize hosts = _hosts;

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
    _authentication = [[RemotingAuthentication alloc] init];
    _authentication.delegate = self;
    _hosts = nil;
    _hostListFetcher = nil;
    // TODO(nicholss): This might need a pointer back to the service.
    _clientRuntimeDelegate =
        new remoting::IosClientRuntimeDelegate();
    [self runtime]->SetDelegate(_clientRuntimeDelegate);
  }
  return self;
}

#pragma mark - RemotingService Implementation

- (void)startHostListFetchWith:(NSString*)accessToken {
  if (!_hostListFetcher) {
    _hostListFetcher = new remoting::HostListFetcher(
        remoting::ChromotingClientRuntime::GetInstance()->url_requester());
  }
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
        [self hostListUpdated];
      }));
}

#pragma mark - Notifications

- (void)hostListUpdated {
  [[NSNotificationCenter defaultCenter] postNotificationName:kHostsDidUpdate
                                                      object:self
                                                    userInfo:nil];
}

#pragma mark - RemotingAuthenticationDelegate

- (void)userDidUpdate:(UserInfo*)user {
  NSDictionary* userInfo = nil;
  if (user) {
    userInfo = [NSDictionary dictionaryWithObject:user forKey:kUserInfo];
  } else {
    _hosts = nil;
    [self hostListUpdated];
  }
  [[NSNotificationCenter defaultCenter] postNotificationName:kUserDidUpdate
                                                      object:self
                                                    userInfo:userInfo];
}

#pragma mark - Properties

- (NSArray<HostInfo*>*)hosts {
  if ([_authentication.user isAuthenticated]) {
    return _hosts;
  }
  return nil;
}

- (remoting::ChromotingClientRuntime*)runtime {
  return remoting::ChromotingClientRuntime::GetInstance();
}

#pragma mark - Implementation

- (void)requestHostListFetch {
  [_authentication
      callbackWithAccessToken:base::BindBlockArc(^(
                                  remoting::OAuthTokenGetter::Status status,
                                  const std::string& user_email,
                                  const std::string& access_token) {
        NSString* accessToken =
            [NSString stringWithCString:access_token.c_str()
                               encoding:[NSString defaultCStringEncoding]];
        [self startHostListFetchWith:accessToken];
      })];
}

@end
