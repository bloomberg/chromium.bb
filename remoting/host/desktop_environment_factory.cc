// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment_factory.h"

#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/video_frame_capturer.h"

namespace remoting {

DesktopEnvironmentFactory::DesktopEnvironmentFactory() {
}

DesktopEnvironmentFactory::~DesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> DesktopEnvironmentFactory::Create(
    ChromotingHostContext* context) {
  scoped_ptr<AudioCapturer> audio_capturer = AudioCapturer::Create();
  scoped_ptr<EventExecutor> event_executor = EventExecutor::Create(
      context->desktop_task_runner(),
      context->ui_task_runner());
  scoped_ptr<VideoFrameCapturer> video_capturer(VideoFrameCapturer::Create());
  return scoped_ptr<DesktopEnvironment>(new DesktopEnvironment(
      audio_capturer.Pass(),
      event_executor.Pass(),
      video_capturer.Pass()));
}

bool DesktopEnvironmentFactory::SupportsAudioCapture() const {
  return AudioCapturer::IsSupported();
}

}  // namespace remoting
