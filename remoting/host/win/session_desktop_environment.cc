// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_desktop_environment.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/win/session_event_executor.h"

namespace remoting {

SessionDesktopEnvironment::SessionDesktopEnvironment(
    const base::Closure& inject_sas)
    : inject_sas_(inject_sas){
}

SessionDesktopEnvironment::~SessionDesktopEnvironment() {
}

scoped_ptr<EventExecutor> SessionDesktopEnvironment::CreateEventExecutor(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<EventExecutor> event_executor = EventExecutor::Create(
      input_task_runner, ui_task_runner);
  event_executor.reset(new SessionEventExecutorWin(
      input_task_runner, event_executor.Pass(), ui_task_runner, inject_sas_));
  return event_executor.Pass();
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
