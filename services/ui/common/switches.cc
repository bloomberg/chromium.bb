// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/common/switches.h"

namespace ui {
namespace switches {

// WindowServer uses the asynchronous event targeting logic.
const char kUseAsyncEventTargeting[] = "enable-async-event-targeting";

// Initializes X11 in threaded mode, and sets the |override_redirect| flag when
// creating X11 windows. Also, exposes the WindowServerTest interface to clients
// when launched with this flag.
const char kUseTestConfig[] = "use-test-config";

// WindowServer uses the viz hit-test logic (HitTestAggregator and
// HitTestQuery).
const char kUseVizHitTest[] = "use-viz-hit-test";

}  // namespace switches
}  // namespace ui
