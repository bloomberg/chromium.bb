// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_extension_session_manager.h"

#include "remoting/base/capabilities.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_extension_session.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
namespace remoting {

HostExtensionSessionManager::HostExtensionSessionManager(
    const std::vector<HostExtension*>& extensions,
    ClientSessionControl* client_session_control)
    : client_session_control_(client_session_control),
      client_stub_(NULL),
      extensions_(extensions) {
}

HostExtensionSessionManager::~HostExtensionSessionManager() {
}

std::string HostExtensionSessionManager::GetCapabilities() const {
  std::string capabilities;
  for (HostExtensions::const_iterator extension = extensions_.begin();
       extension != extensions_.end(); ++extension) {
    const std::string& capability = (*extension)->capability();
    if (capability.empty()) {
      continue;
    }
    if (!capabilities.empty()) {
      capabilities.append(" ");
    }
    capabilities.append(capability);
  }
  return capabilities;
}

void HostExtensionSessionManager::OnCreateVideoCapturer(
    scoped_ptr<webrtc::DesktopCapturer>* capturer) {
  for (HostExtensionSessions::const_iterator it = extension_sessions_.begin();
      it != extension_sessions_.end(); ++it) {
    if ((*it)->ModifiesVideoPipeline()) {
      (*it)->OnCreateVideoCapturer(capturer);
    }
  }
}

void HostExtensionSessionManager::OnCreateVideoEncoder(
    scoped_ptr<VideoEncoder>* encoder) {
  for (HostExtensionSessions::const_iterator it = extension_sessions_.begin();
      it != extension_sessions_.end(); ++it) {
    if ((*it)->ModifiesVideoPipeline()) {
      (*it)->OnCreateVideoEncoder(encoder);
    }
  }
}

void HostExtensionSessionManager::OnNegotiatedCapabilities(
    protocol::ClientStub* client_stub,
    const std::string& capabilities) {
  DCHECK(client_stub);
  DCHECK(!client_stub_);

  client_stub_ = client_stub;

  bool reset_video_pipeline = false;

  for (HostExtensions::const_iterator extension = extensions_.begin();
       extension != extensions_.end(); ++extension) {
    // If the extension requires a capability that was not negotiated then do
    // not instantiate it.
    if (!(*extension)->capability().empty() &&
        !HasCapability(capabilities, (*extension)->capability())) {
      continue;
    }

    scoped_ptr<HostExtensionSession> extension_session =
        (*extension)->CreateExtensionSession(
            client_session_control_, client_stub_);
    DCHECK(extension_session);

    if (extension_session->ModifiesVideoPipeline()) {
      reset_video_pipeline = true;
    }

    extension_sessions_.push_back(extension_session.release());
  }

  // Re-create the video pipeline if one or more extensions need to modify it.
  if (reset_video_pipeline) {
    client_session_control_->ResetVideoPipeline();
  }
}

bool HostExtensionSessionManager::OnExtensionMessage(
    const protocol::ExtensionMessage& message) {
  for(HostExtensionSessions::const_iterator it = extension_sessions_.begin();
      it != extension_sessions_.end(); ++it) {
    if ((*it)->OnExtensionMessage(
            client_session_control_, client_stub_, message)) {
      return true;
    }
  }
  return false;
}

}  // namespace remoting
