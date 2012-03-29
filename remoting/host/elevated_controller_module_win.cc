// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

// MIDL-generated declarations.
#include <elevated_controller.h>

namespace remoting {

class ElevatedControllerModuleWin
    : public ATL::CAtlExeModuleT<ElevatedControllerModuleWin> {
 public:
  DECLARE_LIBID(LIBID_ChromotingElevatedControllerLib)
};

} // namespace remoting


remoting::ElevatedControllerModuleWin _AtlModule;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int command) {
  return _AtlModule.WinMain(command);
}
