// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_desktop_environment.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/win/session_input_injector.h"

namespace remoting {

SessionDesktopEnvironment::SessionDesktopEnvironment(
    const base::Closure& inject_sas)
    : inject_sas_(inject_sas){
}

SessionDesktopEnvironment::~SessionDesktopEnvironment() {
}

scoped_ptr<InputInjector> SessionDesktopEnvironment::CreateInputInjector(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<InputInjector> input_injector = InputInjector::Create(
      input_task_runner, ui_task_runner);
  input_injector.reset(new SessionInputInjectorWin(
      input_task_runner, input_injector.Pass(), ui_task_runner, inject_sas_));
  return input_injector.Pass();
}

SessionDesktopEnvironmentFactory::SessionDesktopEnvironmentFactory(
    const base::Closure& inject_sas)
    : inject_sas_(inject_sas) {
}

SessionDesktopEnvironmentFactory::~SessionDesktopEnvironmentFactory() {
}

scoped_ptr<DesktopEnvironment> SessionDesktopEnvironmentFactory::Create(
    const std::string& client_jid,
    const base::Closure& disconnect_callback) {
  return scoped_ptr<DesktopEnvironment>(
      new SessionDesktopEnvironment(inject_sas_));
}

}  // namespace remoting
