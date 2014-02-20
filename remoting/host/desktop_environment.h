// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class ScreenCapturer;
}  // namespace webrtc

namespace remoting {

namespace protocol {
class ClientStub;
}  // namespace protocol

class AudioCapturer;
class ClientSessionControl;
class GnubbyAuthHandler;
class InputInjector;
class ScreenControls;

// Provides factory methods for creation of audio/video capturers and event
// executor for a given desktop environment.
class DesktopEnvironment {
 public:
  virtual ~DesktopEnvironment() {}

  // Factory methods used to create audio/video capturers, event executor, and
  // screen controls object for a particular desktop environment.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer() = 0;
  virtual scoped_ptr<InputInjector> CreateInputInjector() = 0;
  virtual scoped_ptr<ScreenControls> CreateScreenControls() = 0;
  virtual scoped_ptr<webrtc::ScreenCapturer> CreateVideoCapturer() = 0;

  // Returns the set of all capabilities supported by |this|.
  virtual std::string GetCapabilities() const = 0;

  // Passes the final set of capabilities negotiated between the client and host
  // to |this|.
  virtual void SetCapabilities(const std::string& capabilities) = 0;

  // Factory method to create a gnubby auth handler suitable for the particular
  // desktop environment.
  virtual scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) = 0;
};

// Used to create |DesktopEnvironment| instances.
class DesktopEnvironmentFactory {
 public:
  virtual ~DesktopEnvironmentFactory() {}

  // Creates an instance of |DesktopEnvironment|. Returns a NULL pointer if
  // the desktop environment could not be created for any reason (if the curtain
  // failed to active for instance). |client_session_control| must outlive
  // the created desktop environment.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) = 0;

  // Enables or disables the curtain mode.
  virtual void SetEnableCurtaining(bool enable) {}

  // Returns |true| if created |DesktopEnvironment| instances support audio
  // capture.
  virtual bool SupportsAudioCapture() const = 0;

  // Enables or disables gnubby authentication.
  virtual void SetEnableGnubbyAuth(bool enable) {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
