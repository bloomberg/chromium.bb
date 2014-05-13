// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/Chromoting_unittests/main_no_arc.h"

#include "base/message_loop/message_loop.h"

namespace remoting {
namespace main_no_arc {

void init() {
  // Declare the pump factory before the test suite can declare it.  The test
  // suite assumed we are running in x86 simulator, but this test project runs
  // on an actual device
  base::MessageLoop::InitMessagePumpForUIFactory(&base::MessagePumpMac::Create);
}

}  // main_no_arc
}  // remoting
