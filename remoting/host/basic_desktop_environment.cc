// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/screen_controls.h"

namespace remoting {

BasicDesktopEnvironment::~BasicDesktopEnvironment() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

scoped_ptr<AudioCapturer> BasicDesktopEnvironment::CreateAudioCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::Create();
}

scoped_ptr<InputInjector> BasicDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return InputInjector::Create(input_task_runner(), ui_task_runner());
}

scoped_ptr<ScreenControls> BasicDesktopEnvironment::CreateScreenControls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<ScreenControls>();
}

scoped_ptr<media::ScreenCapturer>
BasicDesktopEnvironment::CreateVideoCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // The basic desktop environment does not use X DAMAGE, since it is
  // broken on many systems - see http://crbug.com/73423.
  return media::ScreenCapturer::Create();
}

BasicDesktopEnvironment::BasicDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Create the local input monitor.
  local_input_monitor_ = LocalInputMonitor::Create(caller_task_runner_,
                                                   input_task_runner_,
                                                   ui_task_runner_,
                                                   client_session_control);
}

BasicDesktopEnvironmentFactory::BasicDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner) {
}

BasicDesktopEnvironmentFactory::~BasicDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> BasicDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return scoped_ptr<DesktopEnvironment>(
      new BasicDesktopEnvironment(caller_task_runner(),
                                  input_task_runner(),
                                  ui_task_runner(),
                                  client_session_control));
}

bool BasicDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

}  // namespace remoting
