// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SINGLE_WINDOW_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_SINGLE_WINDOW_DESKTOP_ENVIRONMENT_H_

#include "remoting/host/basic_desktop_environment.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// Passed to the ChromotingHost to remote an individual window's contents,
// rather than a whole desktop.
class SingleWindowDesktopEnvironmentFactory
    : public BasicDesktopEnvironmentFactory {
 public:
  SingleWindowDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      webrtc::WindowId window_id);
  virtual ~SingleWindowDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory interface.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) OVERRIDE;

 private:
  webrtc::WindowId window_id_;

  DISALLOW_COPY_AND_ASSIGN(SingleWindowDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SINGLE_WINDOW_DESKTOP_ENVIRONMENT_H_
