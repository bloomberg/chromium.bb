// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/common/switches.h"

namespace ws {
namespace switches {

// Initializes X11 in threaded mode, and sets the |override_redirect| flag when
// creating X11 windows. Also, exposes the WindowServerTest interface to clients
// when launched with this flag.
// NOTE: if you use this, you *must* add a data dep on
// "//services/ws/ime/test_ime_driver".
const char kUseTestConfig[] = "use-test-config";

}  // namespace switches
}  // namespace ws
