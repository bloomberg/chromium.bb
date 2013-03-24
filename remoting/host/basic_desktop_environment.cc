// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include "base/logging.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/session_controller.h"

namespace remoting {

BasicDesktopEnvironment::~BasicDesktopEnvironment() {
  DCHECK(CalledOnValidThread());
}

scoped_ptr<AudioCapturer> BasicDesktopEnvironment::CreateAudioCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) {
  DCHECK(CalledOnValidThread());

  return AudioCapturer::Create();
}

scoped_ptr<InputInjector> BasicDesktopEnvironment::CreateInputInjector(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(CalledOnValidThread());

  return InputInjector::Create(input_task_runner, ui_task_runner);
}

scoped_ptr<SessionController>
BasicDesktopEnvironment::CreateSessionController() {
  DCHECK(CalledOnValidThread());

  return scoped_ptr<SessionController>();
}

scoped_ptr<media::ScreenCapturer> BasicDesktopEnvironment::CreateVideoCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) {
  DCHECK(CalledOnValidThread());

  // The basic desktop environment does not use X DAMAGE, since it is
  // broken on many systems - see http://crbug.com/73423.
  return media::ScreenCapturer::Create();
}

BasicDesktopEnvironment::BasicDesktopEnvironment() {
}

BasicDesktopEnvironmentFactory::BasicDesktopEnvironmentFactory() {
}

BasicDesktopEnvironmentFactory::~BasicDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> BasicDesktopEnvironmentFactory::Create(
    const std::string& client_jid,
    const base::Closure& disconnect_callback) {
  return scoped_ptr<DesktopEnvironment>(new BasicDesktopEnvironment());
}

bool BasicDesktopEnvironmentFactory::SupportsAudioCapture() const {
  return AudioCapturer::IsSupported();
}

}  // namespace remoting
