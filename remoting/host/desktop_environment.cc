// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "remoting/host/capturer.h"
#include "remoting/protocol/input_stub.h"

using remoting::protocol::InputStub;

namespace remoting {

DesktopEnvironment::DesktopEnvironment(Capturer* capturer,
                                       InputStub* input_stub)
    : capturer_(capturer),
      input_stub_(input_stub) {
}

DesktopEnvironment::~DesktopEnvironment() {
}

}  // namespace remoting
