// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/switches.h"

#include "build/build_config.h"

namespace remoting {

const char kElevateSwitchName[] = "elevate";
const char kHelpSwitchName[] = "help";
const char kProcessTypeSwitchName[] = "type";
const char kQuestionSwitchName[] = "?";
const char kVersionSwitchName[] = "version";

const char kProcessTypeController[] = "controller";
const char kProcessTypeDaemon[] = "daemon";
const char kProcessTypeDesktop[] = "desktop";
const char kProcessTypeHost[] = "host";
const char kProcessTypeRdpDesktopSession[] = "rdp_desktop_session";
const char kProcessTypeEvaluateCapability[] = "evaluate_capability";

const char kEvaluateCapabilitySwitchName[] = "evaluate-type";

#if defined(OS_WIN)
const char kEvaluateD3D[] = "d3d-support";
#endif

const char kParentWindowSwitchName[] = "parent-window";

const char kInputSwitchName[] = "input";
const char kOutputSwitchName[] = "output";

const char kMojoPipeToken[] = "mojo-pipe-token";

}  // namespace remoting
