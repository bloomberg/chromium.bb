// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "remoting/host/host_window_proxy.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/ui_strings.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(OS_POSIX)

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
    base::WeakPtr<ClientSessionControl> client_session_control,
    const UiStrings* ui_strings)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner),
      ui_strings_(ui_strings) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Create the local input monitor.
  local_input_monitor_ = LocalInputMonitor::Create(caller_task_runner_,
                                                   input_task_runner_,
                                                   ui_task_runner_,
                                                   client_session_control);

  // The host UI should be created on the UI thread.
  bool want_user_interface = true;
#if defined(OS_LINUX)
  want_user_interface = false;
#elif defined(OS_MACOSX)
  // Don't try to display any UI on top of the system's login screen as this
  // is rejected by the Window Server on OS X 10.7.4, and prevents the
  // capturer from working (http://crbug.com/140984).

  // TODO(lambroslambrou): Use a better technique of detecting whether we're
  // running in the LoginWindow context, and refactor this into a separate
  // function to be used here and in CurtainMode::ActivateCurtain().
  want_user_interface = getuid() != 0;
#endif  // OS_MACOSX

  // Create the disconnect window.
  if (want_user_interface) {
    disconnect_window_ = HostWindow::CreateDisconnectWindow(*ui_strings_);
    disconnect_window_.reset(new HostWindowProxy(
        caller_task_runner_,
        ui_task_runner_,
        disconnect_window_.Pass()));
    disconnect_window_->Start(client_session_control);
  }
}

BasicDesktopEnvironmentFactory::BasicDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const UiStrings& ui_strings)
    : caller_task_runner_(caller_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner),
      ui_strings_(ui_strings) {
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
                                  client_session_control,
                                  &ui_strings_));
}

bool BasicDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

}  // namespace remoting
