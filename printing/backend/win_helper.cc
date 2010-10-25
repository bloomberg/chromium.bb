// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/win_helper.h"


#include <windows.h>

namespace printing {

bool InitXPSModule() {
  HMODULE prntvpt_module = LoadLibrary(L"prntvpt.dll");
  return (NULL != prntvpt_module);
}

}  // namespace printing
