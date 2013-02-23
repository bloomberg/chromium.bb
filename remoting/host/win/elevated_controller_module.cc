// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include "remoting/host/win/elevated_controller.h"

namespace remoting {

class ElevatedControllerModule
    : public ATL::CAtlExeModuleT<ElevatedControllerModule> {
 public:
  DECLARE_LIBID(LIBID_ChromotingElevatedControllerLib)
};

int ElevatedControllerMain() {
  ElevatedControllerModule module;
  return module.WinMain(SW_HIDE);
}

} // namespace remoting
