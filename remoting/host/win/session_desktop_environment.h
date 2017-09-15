// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/me2me_desktop_environment.h"

namespace remoting {

// Used to create audio/video capturers and event executor that are compatible
// with Windows sessions.
class SessionDesktopEnvironment : public Me2MeDesktopEnvironment {
 public:
  ~SessionDesktopEnvironment() override;

  // DesktopEnvironment implementation.
  std::unique_ptr<InputInjector> CreateInputInjector() override;

 private:
  friend class SessionDesktopEnvironmentFactory;
  SessionDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      ui::SystemInputInjectorFactory* system_input_injector_factory,
      const base::Closure& inject_sas,
      const base::Closure& lock_workstation,
      const DesktopEnvironmentOptions& options);

  // Used to ask the daemon to inject Secure Attention Sequence.
  base::Closure inject_sas_;

  // Used to lock the workstation for the current session.
  base::Closure lock_workstation_;

  DISALLOW_COPY_AND_ASSIGN(SessionDesktopEnvironment);
};

// Used to create |SessionDesktopEnvironment| instances.
class SessionDesktopEnvironmentFactory : public Me2MeDesktopEnvironmentFactory {
 public:
  SessionDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      ui::SystemInputInjectorFactory* system_input_injector_factory,
      const base::Closure& inject_sas,
      const base::Closure& lock_workstation);
  ~SessionDesktopEnvironmentFactory() override;

  // DesktopEnvironmentFactory implementation.
  std::unique_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control,
      const DesktopEnvironmentOptions& options) override;

 private:
  // Used to ask the daemon to inject Secure Attention Sequence.
  base::Closure inject_sas_;

  // Used to lock the workstation for the current session.
  base::Closure lock_workstation_;

  DISALLOW_COPY_AND_ASSIGN(SessionDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_
