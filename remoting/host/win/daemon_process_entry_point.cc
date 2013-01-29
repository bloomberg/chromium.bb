// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "remoting/host/win/daemon_process_main.h"

// The entry point of the daemon process binary. In order to be really small
// the app doesn't link against the CRT.
void DaemonProcessEntryPoint() {
  int exit_code = remoting::DaemonProcessMain();
  ExitProcess(exit_code);
}
