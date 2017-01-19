// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/client/ios/bridge/client_proxy.h"

#import "remoting/base/string_resources.h"
#import "remoting/client/ios/bridge/client_proxy_delegate_wrapper.h"
#import "remoting/client/ios/host_preferences.h"
#import "remoting/client/ios/utility.h"
#import "remoting/proto/control.pb.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

namespace {
// The value indicating a successful connection has been established via a call
// to ReportConnectionStatus.
const static int kSuccessfulConnection = 3;

NSString* GetStatusMsg(remoting::protocol::ConnectionToHost::State code) {
  switch (code) {
    case remoting::protocol::ConnectionToHost::INITIALIZING:
      return CRD_LOCALIZED_STRING(IDS_FOOTER_WAITING);
    case remoting::protocol::ConnectionToHost::CONNECTING:
      return CRD_LOCALIZED_STRING(IDS_FOOTER_CONNECTING);
    case remoting::protocol::ConnectionToHost::AUTHENTICATED:
      // TODO(nicholss): This says "Working..."
      return CRD_LOCALIZED_STRING(IDS_WORKING);
    case remoting::protocol::ConnectionToHost::CONNECTED:
      // TODO(nicholss): This says "Connected:"
      return CRD_LOCALIZED_STRING(IDS_LABEL_CONNECTED);
    case remoting::protocol::ConnectionToHost::FAILED:
      // TODO(nicholss): This says "Authentication failed. Please sign in to
      // Chrome again."
      return CRD_LOCALIZED_STRING(IDS_ERROR_AUTHENTICATION_FAILED);
    case remoting::protocol::ConnectionToHost::CLOSED:
      return CRD_LOCALIZED_STRING(IDS_MESSAGE_SESSION_FINISHED);
  }
  return CRD_LOCALIZED_STRING(IDS_ERROR_UNEXPECTED);
}

// Translate a connection error code integer to a NSString description.
NSString* GetErrorMsg(remoting::protocol::ErrorCode code) {
  switch (code) {
    case remoting::protocol::ErrorCode::OK:
      return CRD_LOCALIZED_STRING(IDS_OK);
    case remoting::protocol::ErrorCode::INVALID_ACCOUNT:
      return CRD_LOCALIZED_STRING(IDS_ERROR_INVALID_ACCOUNT);
    case remoting::protocol::ErrorCode::MAX_SESSION_LENGTH:
      return CRD_LOCALIZED_STRING(IDS_ERROR_MAX_SESSION_LENGTH);
    case remoting::protocol::ErrorCode::PEER_IS_OFFLINE:
      return CRD_LOCALIZED_STRING(IDS_ERROR_HOST_IS_OFFLINE);
    case remoting::protocol::ErrorCode::HOST_CONFIGURATION_ERROR:
      return CRD_LOCALIZED_STRING(IDS_ERROR_HOST_CONFIGURATION_ERROR);
    case remoting::protocol::ErrorCode::SESSION_REJECTED:
      return CRD_LOCALIZED_STRING(IDS_ERROR_INVALID_ACCESS_CODE);
    case remoting::protocol::ErrorCode::INCOMPATIBLE_PROTOCOL:
      return CRD_LOCALIZED_STRING(IDS_ERROR_INCOMPATIBLE_PROTOCOL);
    case remoting::protocol::ErrorCode::AUTHENTICATION_FAILED:
      return CRD_LOCALIZED_STRING(IDS_ERROR_INVALID_ACCESS_CODE);
    case remoting::protocol::ErrorCode::CHANNEL_CONNECTION_ERROR:
      return CRD_LOCALIZED_STRING(IDS_ERROR_P2P_FAILURE);
    case remoting::protocol::ErrorCode::SIGNALING_ERROR:
      return CRD_LOCALIZED_STRING(IDS_ERROR_P2P_FAILURE);
    case remoting::protocol::ErrorCode::SIGNALING_TIMEOUT:
      return CRD_LOCALIZED_STRING(IDS_ERROR_P2P_FAILURE);
    case remoting::protocol::ErrorCode::HOST_OVERLOAD:
      return CRD_LOCALIZED_STRING(IDS_ERROR_HOST_OVERLOAD);
    case remoting::protocol::ErrorCode::UNKNOWN_ERROR:
      return CRD_LOCALIZED_STRING(IDS_ERROR_UNEXPECTED);
  }
  return CRD_LOCALIZED_STRING(IDS_ERROR_UNEXPECTED);
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
    [delegate_ connectionStatus:GetStatusMsg(state)];

    if (state == kSuccessfulConnection) {
      [delegate_ connected];
    }
  } else {
    [delegate_ connectionStatus:GetStatusMsg(state)];
    if (error != protocol::ErrorCode::OK) {
      [delegate_ connectionFailed:GetErrorMsg(error)];
    }
  }
}

void ClientProxy::DisplayAuthenticationPrompt(bool pairing_supported) {
  DCHECK(delegate_);
  [delegate_ requestHostPin:pairing_supported];
}

void ClientProxy::CommitPairingCredentials(const std::string& host_id,
                                           const std::string& pair_id,
                                           const std::string& pair_secret) {
  DCHECK(delegate_);
  NSString* nsHostId = base::SysUTF8ToNSString(host_id);
  NSString* nsPairId = base::SysUTF8ToNSString(pair_id);
  NSString* nsPairSecret = base::SysUTF8ToNSString(pair_secret);

  HostPreferences* host = [HostPreferences hostForId:nsHostId];
  host.pairId = nsPairId;
  host.pairSecret = nsPairSecret;

  [host saveToKeychain];
}

void ClientProxy::RedrawCanvas(webrtc::DesktopFrame* buffer) {
  DCHECK(delegate_);
  std::vector<webrtc::DesktopRect> rects;

  for (webrtc::DesktopRegion::Iterator i(buffer->updated_region());
       !i.IsAtEnd(); i.Advance()) {
    rects.push_back(i.rect());
  }

  [delegate_ applyFrame:buffer->size()
                 stride:buffer->stride()
                   data:buffer->data()
                  rects:rects];
}

void ClientProxy::UpdateCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(delegate_);
  const uint8_t* cursor_data = reinterpret_cast<const uint8_t*>(
        cursor_shape.data().data());
  [delegate_ applyCursor:webrtc::DesktopSize(cursor_shape.width(),
                                             cursor_shape.height())
                 hotspot:webrtc::DesktopVector(cursor_shape.hotspot_x(),
                                               cursor_shape.hotspot_y())
              cursorData:const_cast<uint8_t*>(cursor_data)];
}

}  // namespace remoting
