// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/me2me_desktop_environment.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/screen_controls.h"

namespace remoting {

Me2MeDesktopEnvironment::~Me2MeDesktopEnvironment() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

scoped_ptr<ScreenControls> Me2MeDesktopEnvironment::CreateScreenControls() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return scoped_ptr<ScreenControls>(
      new ResizingHostObserver(DesktopResizer::Create()));
}

scoped_ptr<media::ScreenCapturer>
Me2MeDesktopEnvironment::CreateVideoCapturer() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

#if defined(OS_LINUX)
  return media::ScreenCapturer::CreateWithXDamage(true);
#else  // !defined(OS_LINUX)
  return media::ScreenCapturer::Create();
#endif  // !defined(OS_LINUX)
}

Me2MeDesktopEnvironment::Me2MeDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : BasicDesktopEnvironment(caller_task_runner,
                              input_task_runner,
                              ui_task_runner,
                              client_session_control) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
}

Me2MeDesktopEnvironmentFactory::Me2MeDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : BasicDesktopEnvironmentFactory(caller_task_runner,
                                     input_task_runner,
                                     ui_task_runner) {
}

Me2MeDesktopEnvironmentFactory::~Me2MeDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> Me2MeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return scoped_ptr<DesktopEnvironment>(
      new Me2MeDesktopEnvironment(caller_task_runner(),
                                  input_task_runner(),
                                  ui_task_runner(),
                                  client_session_control));
}

}  // namespace remoting
