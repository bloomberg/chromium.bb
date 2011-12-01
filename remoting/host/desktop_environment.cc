// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/event_executor.h"

namespace remoting {

// static
DesktopEnvironment* DesktopEnvironment::Create(ChromotingHostContext* context) {
  scoped_ptr<Capturer> capturer(Capturer::Create());
  scoped_ptr<EventExecutor> event_executor(
      EventExecutor::Create(context->desktop_message_loop(), capturer.get()));

  if (capturer.get() == NULL || event_executor.get() == NULL) {
    LOG(ERROR) << "Unable to create DesktopEnvironment";
    return NULL;
  }

  return new DesktopEnvironment(context,
                                capturer.release(),
                                event_executor.release());
}

DesktopEnvironment::DesktopEnvironment(ChromotingHostContext* context,
                                       Capturer* capturer,
                                       EventExecutor* event_executor)
    : host_(NULL),
      context_(context),
      capturer_(capturer),
      event_executor_(event_executor) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

}  // namespace remoting
