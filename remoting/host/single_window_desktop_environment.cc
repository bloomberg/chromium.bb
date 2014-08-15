// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/single_window_desktop_environment.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/single_window_input_injector.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace remoting {

// Enables capturing and streaming of windows.
class SingleWindowDesktopEnvironment : public BasicDesktopEnvironment {

 public:
  virtual ~SingleWindowDesktopEnvironment();

  // DesktopEnvironment interface.
  virtual scoped_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() OVERRIDE;
  virtual scoped_ptr<InputInjector> CreateInputInjector() OVERRIDE;

 protected:
  friend class SingleWindowDesktopEnvironmentFactory;
  SingleWindowDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      webrtc::WindowId window_id);

 private:
  webrtc::WindowId window_id_;

  DISALLOW_COPY_AND_ASSIGN(SingleWindowDesktopEnvironment);
};

SingleWindowDesktopEnvironment::~SingleWindowDesktopEnvironment() {
}

scoped_ptr<webrtc::DesktopCapturer>
SingleWindowDesktopEnvironment::CreateVideoCapturer() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();
  options.set_use_update_notifications(true);

  scoped_ptr<webrtc::WindowCapturer>window_capturer(
        webrtc::WindowCapturer::Create(options));
  window_capturer->SelectWindow(window_id_);

  return window_capturer.PassAs<webrtc::DesktopCapturer>();
}

scoped_ptr<InputInjector>
SingleWindowDesktopEnvironment::CreateInputInjector() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  scoped_ptr<InputInjector> input_injector(
      InputInjector::Create(input_task_runner(),
                            ui_task_runner()));
  return SingleWindowInputInjector::CreateForWindow(
      window_id_, input_injector.Pass()).PassAs<InputInjector>();
}

SingleWindowDesktopEnvironment::SingleWindowDesktopEnvironment(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    webrtc::WindowId window_id)
    : BasicDesktopEnvironment(caller_task_runner,
                              input_task_runner,
                              ui_task_runner),
      window_id_(window_id) {
}

SingleWindowDesktopEnvironmentFactory::SingleWindowDesktopEnvironmentFactory(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    webrtc::WindowId window_id)
    : BasicDesktopEnvironmentFactory(caller_task_runner,
                                     input_task_runner,
                                     ui_task_runner),
      window_id_(window_id) {
}

SingleWindowDesktopEnvironmentFactory::
    ~SingleWindowDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> SingleWindowDesktopEnvironmentFactory::Create(
    base::WeakPtr<ClientSessionControl> client_session_control) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  scoped_ptr<SingleWindowDesktopEnvironment> desktop_environment(
      new SingleWindowDesktopEnvironment(caller_task_runner(),
                                         input_task_runner(),
                                         ui_task_runner(),
                                         window_id_));
  return desktop_environment.PassAs<DesktopEnvironment>();
}

}  // namespace remoting
