// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_desktop_environment_factory.h"

#include "base/single_thread_task_runner.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/win/session_event_executor.h"

namespace remoting {

SessionDesktopEnvironmentFactory::SessionDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const base::Closure& inject_sas)
    : DesktopEnvironmentFactory(input_task_runner, ui_task_runner),
      inject_sas_(inject_sas) {
}

SessionDesktopEnvironmentFactory::~SessionDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> SessionDesktopEnvironmentFactory::Create(
    ClientSession* client) {
  scoped_ptr<AudioCapturer> audio_capturer = AudioCapturer::Create();
  scoped_ptr<EventExecutor> event_executor = EventExecutor::Create(
      input_task_runner_, ui_task_runner_);
  event_executor.reset(new SessionEventExecutorWin(
      input_task_runner_, event_executor.Pass(), ui_task_runner_, inject_sas_));
  scoped_ptr<VideoFrameCapturer> video_capturer(VideoFrameCapturer::Create());
  return scoped_ptr<DesktopEnvironment>(new DesktopEnvironment(
      audio_capturer.Pass(),
      event_executor.Pass(),
      video_capturer.Pass()));
}

}  // namespace remoting
