// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/basic_desktop_environment.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "remoting/protocol/capability_names.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

#if defined(OS_CHROMEOS)
#include "remoting/host/chromeos/aura_desktop_capturer.h"
#include "remoting/host/chromeos/mouse_cursor_monitor_aura.h"
#endif
#if defined(USE_X11)
#include "remoting/host/linux/x11_util.h"
#endif

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

  return nullptr;
}

scoped_ptr<webrtc::MouseCursorMonitor>
BasicDesktopEnvironment::CreateMouseCursorMonitor() {
  scoped_ptr<webrtc::MouseCursorMonitor> cursor_monitor;

#if defined(OS_CHROMEOS)
  cursor_monitor.reset(new MouseCursorMonitorAura());
#else
  cursor_monitor.reset(webrtc::MouseCursorMonitor::CreateForScreen(
      *desktop_capture_options_, webrtc::kFullDesktopScreenId));
#endif
  return make_scoped_ptr(new MouseCursorMonitorProxy(
      video_capture_task_runner_, std::move(cursor_monitor)));
}

std::string BasicDesktopEnvironment::GetCapabilities() const {
  if (supports_touch_events_)
    return protocol::kTouchEventsCapability;

  return std::string();
}

void BasicDesktopEnvironment::SetCapabilities(const std::string& capabilities) {
}

scoped_ptr<GnubbyAuthHandler> BasicDesktopEnvironment::CreateGnubbyAuthHandler(
    protocol::ClientStub* client_stub) {
  return nullptr;
}

scoped_ptr<webrtc::DesktopCapturer>
BasicDesktopEnvironment::CreateVideoCapturer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_ptr<webrtc::DesktopCapturer> capturer;

#if defined(OS_CHROMEOS)
  capturer.reset(new AuraDesktopCapturer());
#else  // !defined(OS_CHROMEOS)
  capturer.reset(webrtc::ScreenCapturer::Create(*desktop_capture_options_));
#endif  // !defined(OS_CHROMEOS)

  return make_scoped_ptr(new DesktopCapturerProxy(video_capture_task_runner_,
                                                  std::move(capturer)));
}

BasicDesktopEnvironment::BasicDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    bool supports_touch_events)
    : caller_task_runner_(caller_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner),
      desktop_capture_options_(new webrtc::DesktopCaptureOptions(
          webrtc::DesktopCaptureOptions::CreateDefault())),
      supports_touch_events_(supports_touch_events) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
#if defined(USE_X11)
  IgnoreXServerGrabs(desktop_capture_options_->x_display()->display(), true);
#endif
}

BasicDesktopEnvironmentFactory::BasicDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : caller_task_runner_(caller_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      input_task_runner_(input_task_runner),
      ui_task_runner_(ui_task_runner),
      supports_touch_events_(false) {}

BasicDesktopEnvironmentFactory::~BasicDesktopEnvironmentFactory() {}

bool BasicDesktopEnvironmentFactory::SupportsAudioCapture() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return AudioCapturer::IsSupported();
}

}  // namespace remoting
