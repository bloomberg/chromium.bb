// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_BRIDGE_HOST_PROXY_DELEGATE_H_
#define REMOTING_IOS_BRIDGE_HOST_PROXY_DELEGATE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <vector>

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

// Contract to provide for callbacks from the common Chromoting protocol to the
// UI Application.
@protocol ClientProxyDelegate<NSObject>

// HOST request for client to input their PIN.
- (void)requestHostPin:(BOOL)pairingSupported;

// HOST notification that a connection has been successfully opened.
- (void)connected;

// HOST notification for a change in connections status.
- (void)connectionStatus:(NSString*)statusMessage;

// HOST notification that a connection has failed.
- (void)connectionFailed:(NSString*)errorMessage;

// A new Canvas (desktop) update has arrived.
- (void)applyFrame:(const webrtc::DesktopSize&)size
            stride:(NSInteger)stride
              data:(uint8_t*)data
           regions:(const std::vector<webrtc::DesktopRect>&)regions;

// A new Cursor (mouse) update has arrived.
- (void)applyCursor:(const webrtc::DesktopSize&)size
            hotspot:(const webrtc::DesktopVector&)hotspot
         cursorData:(uint8_t*)data;
@end

#endif  // REMOTING_IOS_BRIDGE_HOST_PROXY_DELEGATE_H_