// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/bridge/client_proxy.h"

#import "remoting/ios/data_store.h"
#import "remoting/ios/host_preferences.h"
#import "remoting/ios/bridge/client_proxy_delegate_wrapper.h"

namespace {
// The value indicating a successful connection has been established via a call
// to ReportConnectionStatus
const static int kSuccessfulConnection = 3;

// Translate a connection status code integer to a NSString description
NSString* GetStatusMsgFromInteger(NSInteger code) {
  switch (code) {
    case 0:  // INITIALIZING
      return @"Initializing connection";
    case 1:  // CONNECTING
      return @"Connecting";
    case 2:  // AUTHENTICATED
      return @"Authenticated";
    case 3:  // CONNECTED
      return @"Connected";
    case 4:  // FAILED
      return @"Connection Failed";
    case 5:  // CLOSED
      return @"Connection closed";
    default:
      return @"Unknown connection state";
  }
}

// Translate a connection error code integer to a NSString description
NSString* GetErrorMsgFromInteger(NSInteger code) {
  switch (code) {
    case 1:  // PEER_IS_OFFLINE
      return @"Requested host is offline.";
    case 2:  // SESSION_REJECTED
      return @"Session was rejected by the host.";
    case 3:  // INCOMPATIBLE_PROTOCOL
      return @"Incompatible Protocol.";
    case 4:  // AUTHENTICATION_FAILED
      return @"Authentication Failed.";
    case 5:  // CHANNEL_CONNECTION_ERROR
      return @"Channel Connection Error";
    case 6:  // SIGNALING_ERROR
      return @"Signaling Error";
    case 7:  // SIGNALING_TIMEOUT
      return @"Signaling Timeout";
    case 8:  // HOST_OVERLOAD
      return @"Host Overload";
    case 9:  // UNKNOWN_ERROR
      return @"An unknown error has occurred, preventing the session "
              "from opening.";
    default:
      return @"An unknown error code has occurred.";
  }
}

}  // namespace

namespace remoting {

ClientProxy::ClientProxy(ClientProxyDelegateWrapper* wrapper) {
  delegate_ = [wrapper delegate];
}

void ClientProxy::ReportConnectionStatus(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(delegate_);
  if (state <= kSuccessfulConnection && error == protocol::ErrorCode::OK) {
    // Report Progress
    [delegate_ connectionStatus:GetStatusMsgFromInteger(state)];

    if (state == kSuccessfulConnection) {
      [delegate_ connected];
    }
  } else {
    [delegate_ connectionStatus:GetStatusMsgFromInteger(state)];
    if (error != protocol::ErrorCode::OK) {
      [delegate_ connectionFailed:GetErrorMsgFromInteger(error)];
    }
  }
}

void ClientProxy::DisplayAuthenticationPrompt(bool pairing_supported) {
  DCHECK(delegate_);
  [delegate_ requestHostPin:pairing_supported];
}

void ClientProxy::CommitPairingCredentials(const std::string& hostId,
                                           const std::string& pairId,
                                           const std::string& pairSecret) {
  DCHECK(delegate_);
  NSString* nsHostId = [[NSString alloc] initWithUTF8String:hostId.c_str()];
  NSString* nsPairId = [[NSString alloc] initWithUTF8String:pairId.c_str()];
  NSString* nsPairSecret =
      [[NSString alloc] initWithUTF8String:pairSecret.c_str()];

  const HostPreferences* hostPrefs =
      [[DataStore sharedStore] getHostForId:nsHostId];
  if (hostPrefs == nil) {
    hostPrefs = [[DataStore sharedStore] createHost:nsHostId];
  }
  if (hostPrefs) {
    hostPrefs.pairId = nsPairId;
    hostPrefs.pairSecret = nsPairSecret;

    [[DataStore sharedStore] saveChanges];
  }
}

void ClientProxy::RedrawCanvas(const webrtc::DesktopSize& view_size,
                               webrtc::DesktopFrame* buffer,
                               const webrtc::DesktopRegion& region) {
  DCHECK(delegate_);
  std::vector<webrtc::DesktopRect> regions;

  for (webrtc::DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    const webrtc::DesktopRect& rect(i.rect());

    regions.push_back(webrtc::DesktopRect::MakeXYWH(
        rect.left(), rect.top(), rect.width(), rect.height()));
  }

  [delegate_ applyFrame:view_size
                 stride:buffer->stride()
                   data:buffer->data()
                regions:regions];
}

void ClientProxy::UpdateCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(delegate_);
  [delegate_ applyCursor:webrtc::DesktopSize(cursor_shape.width(),
                                             cursor_shape.height())
                 hotspot:webrtc::DesktopVector(cursor_shape.hotspot_x(),
                                               cursor_shape.hotspot_y())
              cursorData:(uint8_t*)cursor_shape.data().c_str()];
}

}  // namespace remoting
