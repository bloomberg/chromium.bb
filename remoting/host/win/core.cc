// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* reserved) {
  if (reason == DLL_PROCESS_ATTACH)
    DisableThreadLibraryCalls(instance);

  return TRUE;
}
