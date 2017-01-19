// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ios/bridge/client_instance.h"

#import "remoting/client/ios/host_preferences.h"
#import "testing/gtest_mac.h"

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/base/constants.h"
#include "remoting/client/ios/bridge/client_proxy.h"
#include "remoting/client/ios/bridge/client_proxy_delegate_wrapper.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

@interface ClientProxyDelegateForClientInstanceTester
    : NSObject<ClientProxyDelegate>

- (void)resetDidReceiveSomething;

// Validating what was received is outside of the scope for this test unit.  See
// ClientProxyUnittest for those tests.
@property(nonatomic, assign) BOOL didReceiveSomething;

@end

@implementation ClientProxyDelegateForClientInstanceTester

@synthesize didReceiveSomething = _didReceiveSomething;

- (void)resetDidReceiveSomething {
  _didReceiveSomething = false;
}

- (void)requestHostPin:(BOOL)pairingSupported {
  _didReceiveSomething = true;
}

- (void)connected {
  _didReceiveSomething = true;
}

- (void)connectionStatus:(NSString*)statusMessage {
  _didReceiveSomething = true;
}

- (void)connectionFailed:(NSString*)errorMessage {
  _didReceiveSomething = true;
}

- (void)applyFrame:(const webrtc::DesktopSize&)size
            stride:(NSInteger)stride
              data:(uint8_t*)data
             rects:(const std::vector<webrtc::DesktopRect>&)rects {
  _didReceiveSomething = true;
}

- (void)applyCursor:(const webrtc::DesktopSize&)size
            hotspot:(const webrtc::DesktopVector&)hotspot
         cursorData:(uint8_t*)data {
  _didReceiveSomething = true;
}

@end

namespace remoting {

namespace {

NSString* const kHostId = @"HostIdTest";
NSString* const kPairingId = @"PairingIdTest";
NSString* const kPairingSecret = @"PairingSecretTest";
NSString* const kSecretPin = @"SecretPinTest";
NSString* const kDeviceId = @"TestingDevice";

// TODO(aboone) should be able to call RunLoop().RunUntilIdle() instead but
// MessagePumpUIApplication::DoRun is marked NOTREACHED()
void RunCFMessageLoop() {
  int result;
  do {  // Repeat until no messages remain
    result = CFRunLoopRunInMode(
        kCFRunLoopDefaultMode,
        0,     // Execute queued messages, do not wait for additional messages
        YES);  // Do only one message at a time
  } while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished &&
           result != kCFRunLoopRunTimedOut);
}

void SecretPinCallBack(const std::string& secret) {
  ASSERT_STREQ(base::SysNSStringToUTF8(kSecretPin).c_str(), secret.c_str());
}

}  // namespace

class ClientInstanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    testDelegate_.reset(
        [[ClientProxyDelegateForClientInstanceTester alloc] init]);
    proxy_.reset(new ClientProxy(
        [ClientProxyDelegateWrapper wrapDelegate:testDelegate_]));
    instance_ = new ClientInstance(proxy_->AsWeakPtr(), "", "", "", "", "");
  }

  void TearDown() override {
    // Ensure memory is not leaking
    // Notice Cleanup is safe to call, regardless of if Start() was ever called.
    instance_->Cleanup();
    RunCFMessageLoop();
    // An object on the network thread which owns a reference to |instance_| may
    // be cleaned up 'soon', but not immediately.  Lets wait it out, up to 1
    // second.
    for (int i = 0; i < 100; i++) {
      if (!instance_->HasOneRef()) {
        [NSThread sleepForTimeInterval:.01];
      } else {
        break;
      }
    }

    // Remove the last reference from |instance_|, and destructor is called.
    ASSERT_TRUE(instance_->HasOneRef());
    instance_ = NULL;
  }

  void AssertAcknowledged(BOOL wasAcknowledged) {
    ASSERT_EQ(wasAcknowledged, [testDelegate_ didReceiveSomething]);
    // Reset for the next test
    [testDelegate_ resetDidReceiveSomething];
  }

  void TestStatusAndError(protocol::ConnectionToHost::State state,
                          protocol::ErrorCode error) {
    instance_->OnConnectionState(state, error);
    AssertAcknowledged(true);
  }

  void TestConnectionStatus(protocol::ConnectionToHost::State state) {
    TestStatusAndError(state, protocol::ErrorCode::OK);
    TestStatusAndError(state, protocol::ErrorCode::PEER_IS_OFFLINE);
    TestStatusAndError(state, protocol::ErrorCode::SESSION_REJECTED);
    TestStatusAndError(state, protocol::ErrorCode::INCOMPATIBLE_PROTOCOL);
    TestStatusAndError(state, protocol::ErrorCode::AUTHENTICATION_FAILED);
    TestStatusAndError(state, protocol::ErrorCode::CHANNEL_CONNECTION_ERROR);
    TestStatusAndError(state, protocol::ErrorCode::SIGNALING_ERROR);
    TestStatusAndError(state, protocol::ErrorCode::SIGNALING_TIMEOUT);
    TestStatusAndError(state, protocol::ErrorCode::HOST_OVERLOAD);
    TestStatusAndError(state, protocol::ErrorCode::UNKNOWN_ERROR);
  }

  base::scoped_nsobject<ClientProxyDelegateForClientInstanceTester>
      testDelegate_;
  std::unique_ptr<ClientProxy> proxy_;
  scoped_refptr<ClientInstance> instance_;
};

TEST_F(ClientInstanceTest, Create) {
  // This is a test for memory leaking.  Ensure a completely unused instance of
  // ClientInstance is destructed.

  ASSERT_TRUE(instance_.get() != NULL);
  ASSERT_TRUE(instance_->HasOneRef());
}

TEST_F(ClientInstanceTest, CreateAndStart) {
  // This is a test for memory leaking.  Ensure a properly used instance of
  // ClientInstance is destructed.

  ASSERT_TRUE(instance_.get() != NULL);
  ASSERT_TRUE(instance_->HasOneRef());

  instance_->Start("", "");
  RunCFMessageLoop();
  ASSERT_TRUE(!instance_->HasOneRef());  // more than one
}

TEST_F(ClientInstanceTest, SecretPin) {
  NSString* hostId = kHostId;
  NSString* pairingId = kPairingId;
  NSString* pairingSecret = kPairingSecret;

  HostPreferences* newHost = [HostPreferences hostForId:hostId];
  newHost.pairId = pairingId;
  newHost.pairSecret = pairingSecret;
  [newHost saveToKeychain];

  // Suggesting that our pairing Id is known, but since it is not the correct
  // credentials expect the stored value to be discarded before requesting the
  // PIN.
  instance_ = new ClientInstance(proxy_->AsWeakPtr(), "", "", "",
                                 base::SysNSStringToUTF8(kHostId), "");

  instance_->Start(base::SysNSStringToUTF8(kPairingId),
                   base::SysNSStringToUTF8(kPairingSecret));
  RunCFMessageLoop();

  instance_->FetchSecret(false, base::Bind(&SecretPinCallBack));
  RunCFMessageLoop();
  AssertAcknowledged(true);

  HostPreferences* host = [HostPreferences hostForId:hostId];

  // The pairing information was discarded.
  ASSERT_TRUE([host.pairId isEqualToString:@""]);
  ASSERT_TRUE([host.pairSecret isEqualToString:@""]);

  instance_->ProvideSecret(base::SysNSStringToUTF8(kSecretPin), false,
                           base::SysNSStringToUTF8(kDeviceId));
  RunCFMessageLoop();
}

TEST_F(ClientInstanceTest, NoProxy) {
  // After the proxy is released, we still expect quite a few functions to be
  // able to run, but not produce any output.  Some of these are just being
  // executed for code coverage, the outputs are not pertinent to this test
  // unit.
  proxy_.reset();

  instance_->Start("", "");
  RunCFMessageLoop();

  instance_->FetchSecret(false, base::Bind(&SecretPinCallBack));
  AssertAcknowledged(false);

  instance_->ProvideSecret(base::SysNSStringToUTF8(kSecretPin), false,
                           base::SysNSStringToUTF8(kDeviceId));
  AssertAcknowledged(false);

  instance_->PerformMouseAction(webrtc::DesktopVector(0, 0),
                                webrtc::DesktopVector(0, 0),
                                (protocol::MouseEvent_MouseButton)0, false);
  AssertAcknowledged(false);

  instance_->PerformKeyboardAction(0, false);
  AssertAcknowledged(false);

  instance_->OnConnectionState(protocol::ConnectionToHost::State::CONNECTED,
                               protocol::ErrorCode::OK);
  AssertAcknowledged(false);

  instance_->OnConnectionReady(false);
  AssertAcknowledged(false);

  instance_->OnRouteChanged("", protocol::TransportRoute());
  AssertAcknowledged(false);

  // SetCapabilities requires a host connection to be established
  // instance_->SetCapabilities("");
  // AssertAcknowledged(false);

  instance_->SetPairingResponse(protocol::PairingResponse());
  AssertAcknowledged(false);

  instance_->DeliverHostMessage(protocol::ExtensionMessage());
  AssertAcknowledged(false);

  ASSERT_TRUE(instance_->GetClipboardStub() != NULL);
  ASSERT_TRUE(instance_->GetCursorShapeStub() != NULL);

  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("Test data.");
  instance_->InjectClipboardEvent(event);
  AssertAcknowledged(false);

  instance_->SetCursorShape(protocol::CursorShapeInfo());
  AssertAcknowledged(false);
}

TEST_F(ClientInstanceTest, OnConnectionStateINITIALIZING) {
  TestConnectionStatus(protocol::ConnectionToHost::State::INITIALIZING);
}

TEST_F(ClientInstanceTest, OnConnectionStateCONNECTING) {
  TestConnectionStatus(protocol::ConnectionToHost::State::CONNECTING);
}

TEST_F(ClientInstanceTest, OnConnectionStateAUTHENTICATED) {
  TestConnectionStatus(protocol::ConnectionToHost::State::AUTHENTICATED);
}

TEST_F(ClientInstanceTest, OnConnectionStateCONNECTED) {
  TestConnectionStatus(protocol::ConnectionToHost::State::CONNECTED);
}

TEST_F(ClientInstanceTest, OnConnectionStateFAILED) {
  TestConnectionStatus(protocol::ConnectionToHost::State::FAILED);
}

TEST_F(ClientInstanceTest, OnConnectionStateCLOSED) {
  TestConnectionStatus(protocol::ConnectionToHost::State::CLOSED);
}

TEST_F(ClientInstanceTest, OnConnectionReady) {
  instance_->OnConnectionReady(true);
  AssertAcknowledged(false);
  instance_->OnConnectionReady(false);
  AssertAcknowledged(false);
}

TEST_F(ClientInstanceTest, OnRouteChanged) {
  // Not expecting anything to happen
  protocol::TransportRoute route;

  route.type = protocol::TransportRoute::DIRECT;
  instance_->OnRouteChanged("", route);
  AssertAcknowledged(false);

  route.type = protocol::TransportRoute::STUN;
  instance_->OnRouteChanged("", route);
  AssertAcknowledged(false);

  route.type = protocol::TransportRoute::RELAY;
  instance_->OnRouteChanged("", route);
  AssertAcknowledged(false);
}

TEST_F(ClientInstanceTest, SetCursorShape) {
  instance_->SetCursorShape(protocol::CursorShapeInfo());
  AssertAcknowledged(true);
}

}  // namespace remoting
