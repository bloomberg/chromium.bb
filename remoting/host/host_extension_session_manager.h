// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_
#define REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

namespace webrtc {
class ScreenCapturer;
}

namespace remoting {

class ClientSessionControl;
class HostExtension;
class HostExtensionSession;
class VideoEncoder;

namespace protocol {
class ClientStub;
class ExtensionMessage;
}

// Helper class used to create and manage a set of HostExtensionSession
// instances depending upon the set of registered HostExtensions, and the
// set of capabilities negotiated between client and host.
class HostExtensionSessionManager {
 public:
  // Creates an extension manager for the specified |extensions|.
  HostExtensionSessionManager(const std::vector<HostExtension*>& extensions,
                              ClientSessionControl* client_session_control);
  virtual ~HostExtensionSessionManager();

  // Returns the union of all capabilities supported by registered extensions.
  std::string GetCapabilities();

  // Calls the corresponding hook functions in each extension in turn, to give
  // them an opportunity to wrap or replace video components.
  scoped_ptr<webrtc::ScreenCapturer> OnCreateVideoCapturer(
      scoped_ptr<webrtc::ScreenCapturer> capturer);
  scoped_ptr<VideoEncoder> OnCreateVideoEncoder(
      scoped_ptr<VideoEncoder> encoder);

  // Handles completion of authentication and capabilities negotiation, creating
  // the set of |HostExtensionSession| to match the client's capabilities.
  void OnNegotiatedCapabilities(protocol::ClientStub* client_stub,
                                const std::string& capabilities);

  // Passes |message| to each |HostExtensionSession| in turn until the message
  // is handled, or none remain. Returns true if the message was handled.
  bool OnExtensionMessage(const protocol::ExtensionMessage& message);

 private:
  typedef std::vector<HostExtension*> HostExtensionList;
  typedef ScopedVector<HostExtensionSession> HostExtensionSessionList;

  // Passed to HostExtensionSessions to allow them to send messages,
  // disconnect the session, etc.
  ClientSessionControl* client_session_control_;
  protocol::ClientStub* client_stub_;

  // List of HostExtensions to attach to the session, if it reaches the
  // authenticated state.
  HostExtensionList extensions_;

  // List of HostExtensionSessions, used to handle extension messages.
  HostExtensionSessionList extension_sessions_;

  DISALLOW_COPY_AND_ASSIGN(HostExtensionSessionManager);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_
