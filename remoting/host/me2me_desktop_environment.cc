// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/me2me_desktop_environment.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/curtain_mode.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/gnubby_auth_handler.h"
#include "remoting/host/host_window.h"
#include "remoting/host/host_window.h"
#include "remoting/host/host_window_proxy.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/screen_controls.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(OS_POSIX)

const char kRateLimitResizeRequests[] = "rateLimitResizeRequests";

namespace remoting {

Me2MeDesktopEnvironment::~Me2MeDesktopEnvironment() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

scoped_ptr<ScreenControls> Me2MeDesktopEnvironment::CreateScreenControls() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return scoped_ptr<ScreenControls>(
      new ResizingHostObserver(DesktopResizer::Create()));
}

std::string Me2MeDesktopEnvironment::GetCapabilities() const {
  return kRateLimitResizeRequests;
}

Me2MeDesktopEnvironment::Me2MeDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : BasicDesktopEnvironment(caller_task_runner,
                              input_task_runner,
                              ui_task_runner),
      gnubby_auth_enabled_(false) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
  desktop_capture_options()->set_use_update_notifications(true);
}

scoped_ptr<GnubbyAuthHandler> Me2MeDesktopEnvironment::CreateGnubbyAuthHandler(
    protocol::ClientStub* client_stub) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (gnubby_auth_enabled_)
    return scoped_ptr<GnubbyAuthHandler>(
        GnubbyAuthHandler::Create(client_stub));

  HOST_LOG << "gnubby auth is not enabled";
  return scoped_ptr<GnubbyAuthHandler>();
}

bool Me2MeDesktopEnvironment::InitializeSecurity(
    base::WeakPtr<ClientSessionControl> client_session_control,
    bool curtain_enabled) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Detach the session from the local console if the caller requested.
  if (curtain_enabled) {
    curtain_ = CurtainMode::Create(caller_task_runner(),
                                   ui_task_runner(),
                                   client_session_control);
    bool active = curtain_->Activate();
    if (!active)
      LOG(ERROR) << "Failed to activate the curtain mode.";
    return active;
  }

  // Otherwise, if the session is shared with the local user start monitoring
  // the local input and create the in-session UI.

#if defined(OS_LINUX)
  bool want_user_interface = false;
#elif defined(OS_MACOSX)
  // Don't try to display any UI on top of the system's login screen as this
  // is rejected by the Window Server on OS X 10.7.4, and prevents the
  // capturer from working (http://crbug.com/140984).

  // TODO(lambroslambrou): Use a better technique of detecting whether we're
  // running in the LoginWindow context, and refactor this into a separate
  // function to be used here and in CurtainMode::ActivateCurtain().
  bool want_user_interface = getuid() != 0;
#elif defined(OS_WIN)
  bool want_user_interface = true;
#endif  // defined(OS_WIN)

  // Create the disconnect window.
  if (want_user_interface) {
    // Create the local input monitor.
    local_input_monitor_ = LocalInputMonitor::Create(caller_task_runner(),
                                                     input_task_runner(),
                                                     ui_task_runner(),
                                                     client_session_control);

    disconnect_window_ = HostWindow::CreateDisconnectWindow();
    disconnect_window_.reset(new HostWindowProxy(
        caller_task_runner(),
        ui_task_runner(),
        disconnect_window_.Pass()));
    disconnect_window_->Start(client_session_control);
  }

  return true;
}

void Me2MeDesktopEnvironment::SetEnableGnubbyAuth(bool gnubby_auth_enabled) {
  gnubby_auth_enabled_ = gnubby_auth_enabled;
}

Me2MeDesktopEnvironmentFactory::Me2MeDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : BasicDesktopEnvironmentFactory(caller_task_runner,
                                     input_task_runner,
                                     ui_task_runner),
      curtain_enabled_(false) {
}

Me2MeDesktopEnvironmentFactory::~Me2MeDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> Me2MeDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  scoped_ptr<Me2MeDesktopEnvironment> desktop_environment(
      new Me2MeDesktopEnvironment(caller_task_runner(),
                                  input_task_runner(),
                                  ui_task_runner()));
  if (!desktop_environment->InitializeSecurity(client_session_control,
                                               curtain_enabled_)) {
    return scoped_ptr<DesktopEnvironment>();
  }
  desktop_environment->SetEnableGnubbyAuth(gnubby_auth_enabled_);

  return desktop_environment.PassAs<DesktopEnvironment>();
}

void Me2MeDesktopEnvironmentFactory::SetEnableCurtaining(bool enable) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  curtain_enabled_ = enable;
}

void Me2MeDesktopEnvironmentFactory::SetEnableGnubbyAuth(
    bool gnubby_auth_enabled) {
  gnubby_auth_enabled_ = gnubby_auth_enabled;
}

}  // namespace remoting
