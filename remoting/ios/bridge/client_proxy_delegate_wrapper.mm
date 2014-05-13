// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/bridge/client_proxy_delegate_wrapper.h"

@interface ClientProxyDelegateWrapper (Private)
- (id)initWithDelegate:(id<ClientProxyDelegate>)delegate;
@end

@implementation ClientProxyDelegateWrapper

@synthesize delegate = _delegate;

- (id)initWithDelegate:(id<ClientProxyDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

+ (id)wrapDelegate:(id<ClientProxyDelegate>)delegate {
  return [[ClientProxyDelegateWrapper alloc] initWithDelegate:delegate];
}

@end
