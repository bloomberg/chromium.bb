// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_switches.h"

namespace switches {

// Enables the testing interface for PPAPI.
const char kEnablePepperTesting[] = "enable-pepper-testing";

// Specifies throttling time in milliseconds for PpapiHostMsg_Keepalive IPCs.
const char kPpapiKeepAliveThrottle[] = "ppapi-keep-alive-throttle";

}  // namespace switches
