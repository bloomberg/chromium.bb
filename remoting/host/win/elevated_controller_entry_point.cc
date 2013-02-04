// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "remoting/host/win/elevated_controller_module.h"

// The entry point of the elevated controller binary. In order to be really
// small the app doesn't link against the CRT.
void ElevatedControllerEntryPoint() {
  int exit_code = remoting::ElevatedControllerMain();
  ExitProcess(exit_code);
}
