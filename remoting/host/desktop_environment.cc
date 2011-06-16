// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "remoting/host/capturer.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/curtain.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/local_input_monitor.h"

namespace remoting {

DesktopEnvironment::DesktopEnvironment(Capturer* capturer,
                                       EventExecutor* event_executor,
                                       Curtain* curtain,
                                       DisconnectWindow* disconnect_window,
                                       ContinueWindow* continue_window,
                                       LocalInputMonitor* local_input_monitor)
    : capturer_(capturer),
      event_executor_(event_executor),
      curtain_(curtain),
      disconnect_window_(disconnect_window),
      continue_window_(continue_window),
      local_input_monitor_(local_input_monitor) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

}  // namespace remoting
