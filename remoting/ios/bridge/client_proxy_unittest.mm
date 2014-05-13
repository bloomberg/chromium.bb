// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/bridge/client_proxy.h"

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

#import "remoting/ios/data_store.h"
#import "remoting/ios/bridge/client_proxy_delegate_wrapper.h"

@interface ClientProxyDelegateTester : NSObject<ClientProxyDelegate>
@property(nonatomic, assign) BOOL isConnected;
@property(nonatomic, copy) NSString* statusMessage;
@property(nonatomic, copy) NSString* errorMessage;
@property(nonatomic, assign) BOOL isPairingSupported;
@property(nonatomic, assign) webrtc::DesktopSize size;
@property(nonatomic, assign) NSInteger stride;
@property(nonatomic, assign) uint8_t* data;
@property(nonatomic, assign) std::vector<webrtc::DesktopRect> regions;
@property(nonatomic, assign) webrtc::DesktopVector hotspot;
@end

@implementation ClientProxyDelegateTester

@synthesize isConnected = _isConnected;
@synthesize statusMessage = _statusMessage;
@synthesize errorMessage = _errorMessage;
@synthesize isPairingSupported = _isPairingSupported;
@synthesize size = _size;
@synthesize stride = _stride;
@synthesize data = _data;
@synthesize regions = _regions;
@synthesize hotspot = _hotspot;

- (void)connected {
  _isConnected = true;
}

- (void)connectionStatus:(NSString*)statusMessage {
  _statusMessage = statusMessage;
}

- (void)connectionFailed:(NSString*)errorMessage {
  _errorMessage = errorMessage;
}

- (void)requestHostPin:(BOOL)pairingSupported {
  _isPairingSupported = pairingSupported;
}

- (void)applyFrame:(const webrtc::DesktopSize&)size
            stride:(NSInteger)stride
              data:(uint8_t*)data
           regions:(const std::vector<webrtc::DesktopRect>&)regions {
  _size = size;
  _stride = stride;
  _data = data;
  _regions.assign(regions.begin(), regions.end());
}

- (void)applyCursor:(const webrtc::DesktopSize&)size
            hotspot:(const webrtc::DesktopVector&)hotspot
         cursorData:(uint8_t*)data {
  _size = size;
  _hotspot = hotspot;
  _data = data;
}

@end

namespace remoting {

namespace {

NSString* kStatusINITIALIZING = @"Initializing connection";
NSString* kStatusCONNECTING = @"Connecting";
NSString* kStatusAUTHENTICATED = @"Authenticated";
NSString* kStatusCONNECTED = @"Connected";
NSString* kStatusFAILED = @"Connection Failed";
NSString* kStatusCLOSED = @"Connection closed";
NSString* kStatusDEFAULT = @"Unknown connection state";

NSString* kErrorPEER_IS_OFFLINE = @"Requested host is offline.";
NSString* kErrorSESSION_REJECTED = @"Session was rejected by the host.";
NSString* kErrorINCOMPATIBLE_PROTOCOL = @"Incompatible Protocol.";
NSString* kErrorAUTHENTICATION_FAILED = @"Authentication Failed.";
NSString* kErrorCHANNEL_CONNECTION_ERROR = @"Channel Connection Error";
NSString* kErrorSIGNALING_ERROR = @"Signaling Error";
NSString* kErrorSIGNALING_TIMEOUT = @"Signaling Timeout";
NSString* kErrorHOST_OVERLOAD = @"Host Overload";
NSString* kErrorUNKNOWN_ERROR =
    @"An unknown error has occurred, preventing the session from opening.";
NSString* kErrorDEFAULT = @"An unknown error code has occurred.";

const webrtc::DesktopSize kFrameSize(100, 100);

// Note these are disjoint regions.  Testing intersecting regions is beyond the
// scope of this test class.
const webrtc::DesktopRect kFrameSubRect1 =
    webrtc::DesktopRect::MakeXYWH(0, 0, 10, 10);
const webrtc::DesktopRect kFrameSubRect2 =
    webrtc::DesktopRect::MakeXYWH(11, 11, 10, 10);
const webrtc::DesktopRect kFrameSubRect3 =
    webrtc::DesktopRect::MakeXYWH(22, 22, 10, 10);

const int kCursorHeight = 10;
const int kCursorWidth = 20;
const int kCursorHotSpotX = 4;
const int kCursorHotSpotY = 8;
// |kCursorDataLength| is assumed to be evenly divisible by 4
const int kCursorDataLength = kCursorHeight * kCursorWidth;
const uint32_t kCursorDataPattern = 0xF0E1D2C3;

const std::string kHostName = "ClientProxyHostNameTest";
const std::string kPairingId = "ClientProxyPairingIdTest";
const std::string kPairingSecret = "ClientProxyPairingSecretTest";

}  // namespace

class ClientProxyTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    delegateTester_ = [[ClientProxyDelegateTester alloc] init];
    clientProxy_.reset(new ClientProxy(
        [ClientProxyDelegateWrapper wrapDelegate:delegateTester_]));
  }

  void ResetIsConnected() { delegateTester_.isConnected = false; }

  void TestConnnectionStatus(protocol::ConnectionToHost::State state,
                             NSString* expectedStatusMsg) {
    ResetIsConnected();
    clientProxy_->ReportConnectionStatus(state, protocol::ErrorCode::OK);
    EXPECT_NSEQ(expectedStatusMsg, delegateTester_.statusMessage);

    if (state == protocol::ConnectionToHost::State::CONNECTED) {
      EXPECT_TRUE(delegateTester_.isConnected);
    } else {
      EXPECT_FALSE(delegateTester_.isConnected);
    }

    TestErrorMessages(state, expectedStatusMsg);
  }

  void TestForError(protocol::ConnectionToHost::State state,
                    protocol::ErrorCode errorCode,
                    NSString* expectedStatusMsg,
                    NSString* expectedErrorMsg) {
    ResetIsConnected();
    clientProxy_->ReportConnectionStatus(state, errorCode);
    EXPECT_FALSE(delegateTester_.isConnected);
    EXPECT_NSEQ(expectedStatusMsg, delegateTester_.statusMessage);
    EXPECT_NSEQ(expectedErrorMsg, delegateTester_.errorMessage);
  }

  void TestErrorMessages(protocol::ConnectionToHost::State state,
                         NSString* expectedStatusMsg) {
    TestForError(state,
                 protocol::ErrorCode::AUTHENTICATION_FAILED,
                 expectedStatusMsg,
                 kErrorAUTHENTICATION_FAILED);
    TestForError(state,
                 protocol::ErrorCode::CHANNEL_CONNECTION_ERROR,
                 expectedStatusMsg,
                 kErrorCHANNEL_CONNECTION_ERROR);
    TestForError(state,
                 protocol::ErrorCode::HOST_OVERLOAD,
                 expectedStatusMsg,
                 kErrorHOST_OVERLOAD);
    TestForError(state,
                 protocol::ErrorCode::INCOMPATIBLE_PROTOCOL,
                 expectedStatusMsg,
                 kErrorINCOMPATIBLE_PROTOCOL);
    TestForError(state,
                 protocol::ErrorCode::PEER_IS_OFFLINE,
                 expectedStatusMsg,
                 kErrorPEER_IS_OFFLINE);
    TestForError(state,
                 protocol::ErrorCode::SESSION_REJECTED,
                 expectedStatusMsg,
                 kErrorSESSION_REJECTED);
    TestForError(state,
                 protocol::ErrorCode::SIGNALING_ERROR,
                 expectedStatusMsg,
                 kErrorSIGNALING_ERROR);
    TestForError(state,
                 protocol::ErrorCode::SIGNALING_TIMEOUT,
                 expectedStatusMsg,
                 kErrorSIGNALING_TIMEOUT);
    TestForError(state,
                 protocol::ErrorCode::UNKNOWN_ERROR,
                 expectedStatusMsg,
                 kErrorUNKNOWN_ERROR);
    TestForError(state,
                 static_cast<protocol::ErrorCode>(999),
                 expectedStatusMsg,
                 kErrorDEFAULT);
  }

  void ValidateHost(const std::string& hostName,
                    const std::string& pairingId,
                    const std::string& pairingSecret) {
    DataStore* store = [DataStore sharedStore];
    NSString* hostNameAsNSString =
        [NSString stringWithUTF8String:hostName.c_str()];
    const HostPreferences* host = [store getHostForId:hostNameAsNSString];
    if (host != nil) {
      [store removeHost:host];
    }

    clientProxy_->CommitPairingCredentials(hostName, pairingId, pairingSecret);

    host = [store getHostForId:hostNameAsNSString];

    ASSERT_TRUE(host != nil);
    ASSERT_STREQ(hostName.c_str(), [host.hostId UTF8String]);
    ASSERT_STREQ(pairingId.c_str(), [host.pairId UTF8String]);
    ASSERT_STREQ(pairingSecret.c_str(), [host.pairSecret UTF8String]);
  }

  scoped_ptr<ClientProxy> clientProxy_;
  ClientProxyDelegateTester* delegateTester_;
  ClientProxyDelegateWrapper* delegateWrapper_;
};

TEST_F(ClientProxyTest, ReportConnectionStatusINITIALIZING) {
  TestConnnectionStatus(protocol::ConnectionToHost::State::INITIALIZING,
                        kStatusINITIALIZING);
}

TEST_F(ClientProxyTest, ReportConnectionStatusCONNECTING) {
  TestConnnectionStatus(protocol::ConnectionToHost::State::CONNECTING,
                        kStatusCONNECTING);
}

TEST_F(ClientProxyTest, ReportConnectionStatusAUTHENTICATED) {
  TestConnnectionStatus(protocol::ConnectionToHost::State::AUTHENTICATED,
                        kStatusAUTHENTICATED);
}

TEST_F(ClientProxyTest, ReportConnectionStatusCONNECTED) {
  TestConnnectionStatus(protocol::ConnectionToHost::State::CONNECTED,
                        kStatusCONNECTED);
}

TEST_F(ClientProxyTest, ReportConnectionStatusFAILED) {
  TestConnnectionStatus(protocol::ConnectionToHost::State::FAILED,
                        kStatusFAILED);
}

TEST_F(ClientProxyTest, ReportConnectionStatusCLOSED) {
  TestConnnectionStatus(protocol::ConnectionToHost::State::CLOSED,
                        kStatusCLOSED);
}

TEST_F(ClientProxyTest, ReportConnectionStatusDEFAULT) {
  TestConnnectionStatus(static_cast<protocol::ConnectionToHost::State>(999),
                        kStatusDEFAULT);
}

TEST_F(ClientProxyTest, DisplayAuthenticationPrompt) {
  clientProxy_->DisplayAuthenticationPrompt(true);
  ASSERT_TRUE(delegateTester_.isPairingSupported);
  clientProxy_->DisplayAuthenticationPrompt(false);
  ASSERT_FALSE(delegateTester_.isPairingSupported);
}

TEST_F(ClientProxyTest, CommitPairingCredentialsBasic) {
  ValidateHost("", "", "");
}

TEST_F(ClientProxyTest, CommitPairingCredentialsExtended) {
  ValidateHost(kHostName, kPairingId, kPairingSecret);
}

TEST_F(ClientProxyTest, RedrawCanvasBasic) {

  webrtc::BasicDesktopFrame frame(webrtc::DesktopSize(1, 1));
  webrtc::DesktopRegion regions;
  regions.AddRect(webrtc::DesktopRect::MakeLTRB(0, 0, 1, 1));

  clientProxy_->RedrawCanvas(webrtc::DesktopSize(1, 1), &frame, regions);

  ASSERT_TRUE(webrtc::DesktopSize(1, 1).equals(delegateTester_.size));
  ASSERT_EQ(4, delegateTester_.stride);
  ASSERT_TRUE(delegateTester_.data != NULL);
  ASSERT_EQ(1, delegateTester_.regions.size());
  ASSERT_TRUE(delegateTester_.regions[0].equals(
      webrtc::DesktopRect::MakeLTRB(0, 0, 1, 1)));
}
TEST_F(ClientProxyTest, RedrawCanvasExtended) {

  webrtc::BasicDesktopFrame frame(kFrameSize);
  webrtc::DesktopRegion regions;
  regions.AddRect(kFrameSubRect1);
  regions.AddRect(kFrameSubRect2);
  regions.AddRect(kFrameSubRect3);

  clientProxy_->RedrawCanvas(kFrameSize, &frame, regions);

  ASSERT_TRUE(kFrameSize.equals(delegateTester_.size));
  ASSERT_EQ(kFrameSize.width() * webrtc::DesktopFrame::kBytesPerPixel,
            delegateTester_.stride);
  ASSERT_TRUE(delegateTester_.data != NULL);
  ASSERT_EQ(3, delegateTester_.regions.size());
  ASSERT_TRUE(delegateTester_.regions[0].equals(kFrameSubRect1));
  ASSERT_TRUE(delegateTester_.regions[1].equals(kFrameSubRect2));
  ASSERT_TRUE(delegateTester_.regions[2].equals(kFrameSubRect3));
}

TEST_F(ClientProxyTest, UpdateCursorBasic) {
  protocol::CursorShapeInfo cursor_proto;
  cursor_proto.set_width(1);
  cursor_proto.set_height(1);
  cursor_proto.set_hotspot_x(0);
  cursor_proto.set_hotspot_y(0);

  char data[4];
  memset(data, 0xFF, 4);

  cursor_proto.set_data(data);

  clientProxy_->UpdateCursorShape(cursor_proto);

  ASSERT_EQ(1, delegateTester_.size.width());
  ASSERT_EQ(1, delegateTester_.size.height());
  ASSERT_EQ(0, delegateTester_.hotspot.x());
  ASSERT_EQ(0, delegateTester_.hotspot.y());
  ASSERT_TRUE(delegateTester_.data != NULL);
  for (int i = 0; i < 4; i++) {
    ASSERT_EQ(0xFF, delegateTester_.data[i]);
  }
}

TEST_F(ClientProxyTest, UpdateCursorExtended) {
  protocol::CursorShapeInfo cursor_proto;
  cursor_proto.set_width(kCursorWidth);
  cursor_proto.set_height(kCursorHeight);
  cursor_proto.set_hotspot_x(kCursorHotSpotX);
  cursor_proto.set_hotspot_y(kCursorHotSpotY);

  char data[kCursorDataLength];
  memset_pattern4(data, &kCursorDataPattern, kCursorDataLength);

  cursor_proto.set_data(data);

  clientProxy_->UpdateCursorShape(cursor_proto);

  ASSERT_EQ(kCursorWidth, delegateTester_.size.width());
  ASSERT_EQ(kCursorHeight, delegateTester_.size.height());
  ASSERT_EQ(kCursorHotSpotX, delegateTester_.hotspot.x());
  ASSERT_EQ(kCursorHotSpotY, delegateTester_.hotspot.y());
  ASSERT_TRUE(delegateTester_.data != NULL);
  for (int i = 0; i < kCursorDataLength / 4; i++) {
    ASSERT_TRUE(memcmp(&delegateTester_.data[i * 4], &kCursorDataPattern, 4) ==
                0);
  }
}

}  // namespace remoting