// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_BRIDGE_HOST_PROXY_H_
#define REMOTING_IOS_BRIDGE_HOST_PROXY_H_

#include <string>

#include <objc/objc.h>
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/connection_to_host.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

#if defined(__OBJC__)
@class ClientProxyDelegateWrapper;
#else   // __OBJC__
class ClientProxyDelegateWrapper;
#endif  // __OBJC__

namespace remoting {

// Proxies incoming common Chromoting protocol (HOST) to the UI Application
// (CLIENT).  The HOST will have a Weak reference to call member functions on
// the UI Thread.
class ClientProxy : public base::SupportsWeakPtr<ClientProxy> {
 public:
  ClientProxy(ClientProxyDelegateWrapper* wrapper);

  // Notifies the user of the current connection status.
  void ReportConnectionStatus(protocol::ConnectionToHost::State state,
                              protocol::ErrorCode error);

  // Display a dialog box asking the user to enter a PIN.
  void DisplayAuthenticationPrompt(bool pairing_supported);

  // Saves new pairing credentials to permanent storage.
  void CommitPairingCredentials(const std::string& hostId,
                                const std::string& pairId,
                                const std::string& pairSecret);

  // Delivers the latest image buffer for the canvas.
  void RedrawCanvas(const webrtc::DesktopSize& view_size,
                    webrtc::DesktopFrame* buffer,
                    const webrtc::DesktopRegion& region);

  // Updates cursor.
  void UpdateCursorShape(const protocol::CursorShapeInfo& cursor_shape);

 private:
  // Pointer to the UI application which implements the ClientProxyDelegate.
  // (id) is similar to a (void*)  |delegate_| is set from accepting a
  // strongly typed @interface which wraps the @protocol ClientProxyDelegate.
  // see comments for host_proxy_delegate_wrapper.h
  id delegate_;

  DISALLOW_COPY_AND_ASSIGN(ClientProxy);
};

}  // namespace remoting

#endif  // REMOTING_IOS_BRIDGE_HOST_PROXY_H_
