// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/me2me_desktop_environment.h"

#include "base/logging.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/session_controller.h"

namespace remoting {

Me2MeDesktopEnvironment::~Me2MeDesktopEnvironment() {
  DCHECK(CalledOnValidThread());
}

scoped_ptr<SessionController>
Me2MeDesktopEnvironment::CreateSessionController() {
  DCHECK(CalledOnValidThread());

  scoped_ptr<SessionController> session_controller(
      new ResizingHostObserver(DesktopResizer::Create()));
  return session_controller.Pass();
}

scoped_ptr<media::ScreenCapturer> Me2MeDesktopEnvironment::CreateVideoCapturer(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) {
  DCHECK(CalledOnValidThread());

#if defined(OS_LINUX)
  return media::ScreenCapturer::CreateWithXDamage(true);
#else  // !defined(OS_LINUX)
  return media::ScreenCapturer::Create();
#endif  // !defined(OS_LINUX)
}

Me2MeDesktopEnvironment::Me2MeDesktopEnvironment() {
}

Me2MeDesktopEnvironmentFactory::Me2MeDesktopEnvironmentFactory() {
}

Me2MeDesktopEnvironmentFactory::~Me2MeDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> Me2MeDesktopEnvironmentFactory::Create(
    const std::string& client_jid,
    const base::Closure& disconnect_callback) {
  return scoped_ptr<DesktopEnvironment>(new Me2MeDesktopEnvironment());
}

}  // namespace remoting
