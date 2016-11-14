// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_BRIDGE_HOST_PROXY_DELEGATE_WRAPPER_H_
#define REMOTING_CLIENT_IOS_BRIDGE_HOST_PROXY_DELEGATE_WRAPPER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <vector>

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#import "remoting/client/ios/bridge/client_proxy_delegate.h"

// Wraps ClientProxyDelegate in a class so C++ can accept a strongly typed
// pointer.  C++ does not understand the id<> convention of passing around a
// OBJ_C @protocol pointer.  So the @protocol is wrapped and the class is passed
// around.  After accepting an instance of ClientProxyDelegateWrapper, the
// @protocol can be referenced as type (id), which is similar to a (void*).

@interface ClientProxyDelegateWrapper : NSObject

@property(nonatomic, retain, readonly) id<ClientProxyDelegate> delegate;

- (id)init __unavailable;

+ (id)wrapDelegate:(id<ClientProxyDelegate>)delegate;

@end

#endif  // REMOTING_CLIENT_IOS_BRIDGE_HOST_PROXY_DELEGATE_WRAPPER_H_
