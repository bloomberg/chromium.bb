// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "remoting/host/capturer.h"
#include "remoting/host/curtain.h"
#include "remoting/host/event_executor.h"

namespace remoting {

DesktopEnvironment::DesktopEnvironment(Capturer* capturer,
                                       EventExecutor* event_executor,
                                       Curtain* curtain)
    : capturer_(capturer),
      event_executor_(event_executor),
      curtain_(curtain) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

}  // namespace remoting
