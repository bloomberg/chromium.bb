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

namespace media {
class ScreenCapturer;
}  // namespace media

namespace remoting {

class AudioCapturer;
class ClientSessionControl;
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
  virtual scoped_ptr<media::ScreenCapturer> CreateVideoCapturer() = 0;
};

// Used to create |DesktopEnvironment| instances.
class DesktopEnvironmentFactory {
 public:
  virtual ~DesktopEnvironmentFactory() {}

  // Creates an instance of |DesktopEnvironment|. |client_session_control| must
  // outlive |this|.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) = 0;

  // Returns |true| if created |DesktopEnvironment| instances support audio
  // capture.
  virtual bool SupportsAudioCapture() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
