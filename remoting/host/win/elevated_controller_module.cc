// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/elevated_controller_module.h"

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "remoting/base/breakpad.h"
#include "remoting/host/logging.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/elevated_controller.h"

namespace remoting {

class ElevatedControllerModule
    : public ATL::CAtlExeModuleT<ElevatedControllerModule> {
 public:
  DECLARE_LIBID(LIBID_ChromotingElevatedControllerLib)
};

int ElevatedControllerMain() {
#ifdef OFFICIAL_BUILD
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // OFFICIAL_BUILD

  CommandLine::Init(0, NULL);

  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  InitHostLogging();

  ElevatedControllerModule module;
  return module.WinMain(SW_HIDE);
}

} // namespace remoting
