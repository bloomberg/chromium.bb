// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "remoting/host/remoting_me2me_host.h"

// The entry point of the host process binary. In order to be really small
// the app doesn't link against the CRT.
void HostProcessEntryPoint() {
  int exit_code = remoting::HostProcessMain(0, NULL);
  ExitProcess(exit_code);
}
