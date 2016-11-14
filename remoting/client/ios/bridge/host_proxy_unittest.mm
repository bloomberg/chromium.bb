// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/bridge/host_proxy.h"

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

namespace remoting {

class HostProxyTest : public ::testing::Test {
 protected:
  void SetUp() override { hostProxy_ = [[HostProxy alloc] init]; }

  void CallPassThroughFunctions() {
    [hostProxy_ mouseAction:webrtc::DesktopVector(0, 0)
                 wheelDelta:webrtc::DesktopVector(0, 0)
                whichButton:0
                 buttonDown:NO];
    [hostProxy_ keyboardAction:0 keyDown:NO];
  }

  HostProxy* hostProxy_;
};

TEST_F(HostProxyTest, ConnectDisconnect) {
  CallPassThroughFunctions();

  ASSERT_FALSE([hostProxy_ isConnected]);
  [hostProxy_ connectToHost:@""
                  authToken:@""
                   jabberId:@""
                     hostId:@""
                  publicKey:@""
                   delegate:nil];
  ASSERT_TRUE([hostProxy_ isConnected]);

  CallPassThroughFunctions();

  [hostProxy_ disconnectFromHost];
  ASSERT_FALSE([hostProxy_ isConnected]);

  CallPassThroughFunctions();
}

}  // namespace remoting
