// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_desktop_environment.h"

#include <utility>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/win/session_input_injector.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace remoting {

SessionDesktopEnvironment::~SessionDesktopEnvironment() {}

scoped_ptr<InputInjector> SessionDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return make_scoped_ptr(new SessionInputInjectorWin(
      input_task_runner(),
      InputInjector::Create(input_task_runner(), ui_task_runner()),
      ui_task_runner(), inject_sas_));
}

SessionDesktopEnvironment::SessionDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const base::Closure& inject_sas,
    bool supports_touch_events)
    : Me2MeDesktopEnvironment(caller_task_runner,
                              video_capture_task_runner,
                              input_task_runner,
                              ui_task_runner,
                              supports_touch_events),
      inject_sas_(inject_sas) {}

SessionDesktopEnvironmentFactory::SessionDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const base::Closure& inject_sas)
    : Me2MeDesktopEnvironmentFactory(caller_task_runner,
                                     video_capture_task_runner,
                                     input_task_runner,
                                     ui_task_runner),
      inject_sas_(inject_sas) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
}

SessionDesktopEnvironmentFactory::~SessionDesktopEnvironmentFactory() {}

scoped_ptr<DesktopEnvironment> SessionDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  scoped_ptr<SessionDesktopEnvironment> desktop_environment(
      new SessionDesktopEnvironment(caller_task_runner(),
                                    video_capture_task_runner(),
                                    input_task_runner(), ui_task_runner(),
                                    inject_sas_, supports_touch_events()));
  if (!desktop_environment->InitializeSecurity(client_session_control,
                                               curtain_enabled())) {
    return nullptr;
  }

  return std::move(desktop_environment);
}

}  // namespace remoting
