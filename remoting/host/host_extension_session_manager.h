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
class DesktopCapturer;
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
  typedef std::vector<HostExtension*> HostExtensions;

  // Creates an extension manager for the specified |extensions|.
  HostExtensionSessionManager(const HostExtensions& extensions,
                              ClientSessionControl* client_session_control);
  virtual ~HostExtensionSessionManager();

  // Returns the union of all capabilities supported by registered extensions.
  std::string GetCapabilities() const;

  // Calls the corresponding hook functions for each extension, to allow them
  // to wrap or replace video pipeline components. Only extensions which return
  // true from ModifiesVideoPipeline() will be called.
  // The order in which extensions are called is undefined.
  void OnCreateVideoCapturer(scoped_ptr<webrtc::DesktopCapturer>* capturer);
  void OnCreateVideoEncoder(scoped_ptr<VideoEncoder>* encoder);

  // Handles completion of authentication and capabilities negotiation, creating
  // the set of HostExtensionSessions to match the client's capabilities.
  void OnNegotiatedCapabilities(protocol::ClientStub* client_stub,
                                const std::string& capabilities);

  // Passes |message| to each HostExtensionSession in turn until the message
  // is handled, or none remain. Returns true if the message was handled.
  // It is not valid for more than one extension to handle the same message.
  bool OnExtensionMessage(const protocol::ExtensionMessage& message);

 private:
  typedef ScopedVector<HostExtensionSession> HostExtensionSessions;

  // Passed to HostExtensionSessions to allow them to send messages,
  // disconnect the session, etc.
  ClientSessionControl* client_session_control_;
  protocol::ClientStub* client_stub_;

  // The HostExtensions to instantiate for the session, if it reaches the
  // authenticated state.
  HostExtensions extensions_;

  // The instantiated HostExtensionSessions, used to handle extension messages.
  HostExtensionSessions extension_sessions_;

  DISALLOW_COPY_AND_ASSIGN(HostExtensionSessionManager);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXTENSION_SESSION_MANAGER_H_
