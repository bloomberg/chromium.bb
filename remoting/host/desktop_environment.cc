// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/event_executor.h"

#if defined(OS_WIN)
#include "remoting/host/session_event_executor_win.h"
#endif

namespace remoting {

// static
scoped_ptr<DesktopEnvironment> DesktopEnvironment::Create(
    ChromotingHostContext* context) {
  scoped_ptr<Capturer> capturer(Capturer::Create());
  scoped_ptr<EventExecutor> event_executor =
      EventExecutor::Create(context->desktop_message_loop(),
                            context->ui_message_loop(),
                            capturer.get());

  if (capturer.get() == NULL || event_executor.get() == NULL) {
    LOG(ERROR) << "Unable to create DesktopEnvironment";
    return scoped_ptr<DesktopEnvironment>();
  }

  return scoped_ptr<DesktopEnvironment>(
      new DesktopEnvironment(context,
                             capturer.Pass(),
                             event_executor.Pass()));
}

// static
scoped_ptr<DesktopEnvironment> DesktopEnvironment::CreateForService(
    ChromotingHostContext* context) {
  scoped_ptr<Capturer> capturer(Capturer::Create());
  scoped_ptr<EventExecutor> event_executor =
      EventExecutor::Create(context->desktop_message_loop(),
                            context->ui_message_loop(),
                            capturer.get());

  if (capturer.get() == NULL || event_executor.get() == NULL) {
    LOG(ERROR) << "Unable to create DesktopEnvironment";
    return scoped_ptr<DesktopEnvironment>();
  }

#if defined(OS_WIN)
  event_executor.reset(new SessionEventExecutorWin(
      context->desktop_message_loop(),
      context->file_message_loop(),
      event_executor.Pass()));
#endif

  return scoped_ptr<DesktopEnvironment>(
      new DesktopEnvironment(context,
                             capturer.Pass(),
                             event_executor.Pass()));
}

// static
scoped_ptr<DesktopEnvironment> DesktopEnvironment::CreateFake(
    ChromotingHostContext* context,
    scoped_ptr<Capturer> capturer,
    scoped_ptr<EventExecutor> event_executor) {
  return scoped_ptr<DesktopEnvironment>(
      new DesktopEnvironment(context,
                             capturer.Pass(),
                             event_executor.Pass()));
}

DesktopEnvironment::DesktopEnvironment(
    ChromotingHostContext* context,
    scoped_ptr<Capturer> capturer,
    scoped_ptr<EventExecutor> event_executor)
    : host_(NULL),
      context_(context),
      capturer_(capturer.Pass()),
      event_executor_(event_executor.Pass()) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

void DesktopEnvironment::OnSessionStarted() {
  event_executor_->OnSessionStarted();
}

void DesktopEnvironment::OnSessionFinished() {
  event_executor_->OnSessionFinished();
}

}  // namespace remoting
