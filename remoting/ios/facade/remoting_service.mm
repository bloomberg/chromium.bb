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
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/domain/user_info.h"
#import "remoting/ios/facade/host_info.h"
#import "remoting/ios/facade/host_list_fetcher.h"
#import "remoting/ios/facade/ios_client_runtime_delegate.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/keychain_wrapper.h"

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_impl.h"

static NSString* const kCRDAuthenticatedUserEmailKey =
    @"kCRDAuthenticatedUserEmailKey";

NSString* const kHostListFetchDidFail = @"kHostListFetchDidFail";
NSString* const kHostListFetchFailureReasonKey = @"kHostListFetchFailureReason";

NSString* const kHostListStateDidChange = @"kHostListStateDidChange";

NSString* const kUserDidUpdate = @"kUserDidUpdate";
NSString* const kUserInfo = @"kUserInfo";

@interface RemotingService ()<RemotingAuthenticationDelegate> {
  id<RemotingAuthentication> _authentication;
  std::unique_ptr<remoting::HostListFetcher> _hostListFetcher;

  // TODO(yuweih): It's suspicious to use a raw C++ pointer here. Investigate
  // its lifetime in ChromotingRuntime and change to unique_ptr if possible.
  remoting::IosClientRuntimeDelegate* _clientRuntimeDelegate;
}
@end

@implementation RemotingService

@synthesize hosts = _hosts;
@synthesize hostListState = _hostListState;
@synthesize lastFetchFailureReason = _lastFetchFailureReason;

// RemotingService is a singleton.
+ (RemotingService*)instance {
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
    _hosts = nil;
    // TODO(yuweih): Maybe better to just cancel the previous request.
    _hostListState = HostListStateNotFetched;
    _lastFetchFailureReason = HostListFetchFailureReasonNoFailure;
    // TODO(nicholss): This might need a pointer back to the service.
    _clientRuntimeDelegate =
        new remoting::IosClientRuntimeDelegate();
    [self runtime]->SetDelegate(_clientRuntimeDelegate);
  }
  return self;
}

#pragma mark - RemotingService Implementation

- (void)startHostListFetchWith:(NSString*)accessToken {
  if (_hostListState == HostListStateFetching) {
    return;
  }
  [self setHostListState:HostListStateFetching];
  if (!_hostListFetcher) {
    _hostListFetcher.reset(new remoting::HostListFetcher(
        remoting::ChromotingClientRuntime::GetInstance()->url_requester()));
  }
  _hostListFetcher->RetrieveHostlist(
      base::SysNSStringToUTF8(accessToken),
      base::BindBlockArc(^(int responseCode,
                           const std::vector<remoting::HostInfo>& hostlist) {
        if (responseCode != net::HTTP_OK) {
          if (responseCode !=
              remoting::HostListFetcher::RESPONSE_CODE_CANCELLED) {
            LOG(WARNING) << "Failed to fetch host list. Response code: "
                         << responseCode;
          }

          if (responseCode == net::HTTP_UNAUTHORIZED) {
            [[RemotingService instance].authentication logout];
          }
          // TODO(nicholss): There are more |responseCode|s that we might want
          // to trigger on, look into that later.

          [self setHostListState:HostListStateNotFetched];
          _hosts = nil;
          return;
        }

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
          // TODO(nicholss): Not yet integrated: createdTime, kind,
          // offlineReason. Add them as the app will need this info.
          HostInfo* host = [[HostInfo alloc] init];
          host.hostId = base::SysUTF8ToNSString(host_info.host_id);
          host.hostName = base::SysUTF8ToNSString(host_info.host_name);
          host.hostOs = base::SysUTF8ToNSString(host_info.host_os);
          host.hostOsVersion =
              base::SysUTF8ToNSString(host_info.host_os_version);
          host.hostVersion = base::SysUTF8ToNSString(host_info.host_version);
          host.jabberId = base::SysUTF8ToNSString(host_info.host_jid);
          host.publicKey = base::SysUTF8ToNSString(host_info.public_key);
          host.status = base::SysUTF8ToNSString(status);
          host.updatedTime = base::SysUTF16ToNSString(
              base::TimeFormatShortDateAndTime(host_info.updated_time));
          host.offlineReason =
              base::SysUTF8ToNSString(host_info.offline_reason);
          [hosts addObject:host];
        }
        _hosts = hosts;
        [self setHostListState:HostListStateFetched];
      }));
}

- (void)setHostListState:(HostListState)state {
  if (state == _hostListState) {
    return;
  }
  if (state == HostListStateFetching || state == HostListStateFetched) {
    _lastFetchFailureReason = HostListFetchFailureReasonNoFailure;
  }
  _hostListState = state;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHostListStateDidChange
                    object:self
                  userInfo:nil];
}

#pragma mark - RemotingAuthenticationDelegate

- (void)userDidUpdate:(UserInfo*)user {
  NSDictionary* userInfo = nil;
  if (_hostListFetcher) {
    _hostListFetcher->CancelFetch();
  }
  [self setHostListState:HostListStateNotFetched];
  if (user) {
    userInfo = [NSDictionary dictionaryWithObject:user forKey:kUserInfo];
    [self requestHostListFetch];
    [_authentication
        callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                  NSString* userEmail, NSString* accessToken) {
          _clientRuntimeDelegate->SetAuthToken(
              base::SysNSStringToUTF8(accessToken));
        }];
  } else {
    _hosts = nil;
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
      callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                NSString* userEmail, NSString* accessToken) {
        if (status == RemotingAuthenticationStatusSuccess) {
          [self startHostListFetchWith:accessToken];
          return;
        }

        switch (status) {
          case RemotingAuthenticationStatusNetworkError:
            _lastFetchFailureReason = HostListFetchFailureReasonNetworkError;
            break;
          case RemotingAuthenticationStatusAuthError:
            _lastFetchFailureReason = HostListFetchFailureReasonAuthError;
            break;
          default:
            _lastFetchFailureReason = HostListFetchFailureReasonUnknown;
        }
        [NSNotificationCenter.defaultCenter
            postNotificationName:kHostListFetchDidFail
                          object:self
                        userInfo:@{
                          kHostListFetchFailureReasonKey :
                              @(_lastFetchFailureReason)
                        }];
      }];
}

- (void)setAuthentication:(id<RemotingAuthentication>)authentication {
  DCHECK(_authentication == nil);
  authentication.delegate = self;
  _authentication = authentication;
}

- (id<RemotingAuthentication>)authentication {
  DCHECK(_authentication != nil);
  return _authentication;
}

@end
