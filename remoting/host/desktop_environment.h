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

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AudioCapturer;
class EventExecutor;
class VideoFrameCapturer;

// Provides factory methods for creation of audio/video capturers and event
// executor for a given desktop environment.
class DesktopEnvironment {
 public:
  virtual ~DesktopEnvironment() {}

  // Factory methods used to create audio/video capturers and event executor for
  // a particular desktop environment.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) = 0;
  virtual scoped_ptr<EventExecutor> CreateEventExecutor(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) = 0;
  virtual scoped_ptr<VideoFrameCapturer> CreateVideoCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) = 0;
};

// Used to create |DesktopEnvironment| instances.
class DesktopEnvironmentFactory {
 public:
  virtual ~DesktopEnvironmentFactory() {}

  // Creates an instance of |DesktopEnvironment|. |disconnect_callback| may be
  // used by the DesktopEnvironment to disconnect the client session.
  virtual scoped_ptr<DesktopEnvironment> Create(
      const std::string& client_jid,
      const base::Closure& disconnect_callback) = 0;

  // Returns |true| if created |DesktopEnvironment| instances support audio
  // capture.
  virtual bool SupportsAudioCapture() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
