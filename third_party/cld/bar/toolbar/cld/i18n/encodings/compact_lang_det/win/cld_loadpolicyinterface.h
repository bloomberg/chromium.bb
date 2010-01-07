// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOADPOLICYINTERFACE_H_
#define BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOADPOLICYINTERFACE_H_

#include <windows.h>

class CldServiceInterface;

// Loading policy for the object implementing CldServiceInterface.
// Checks if the corresponding component is present and valid, calls
// cld_service Load() function with the actual component file name.
class CldLoadPolicyInterface {
 public:
  virtual ~CldLoadPolicyInterface() {}

  // Loads cld_service if necessary. Does nothing and returns success code
  // (ERROR_SUCCESS) if cld_service is already loaded.
  // [in] cld_service - instance to verify loaded status. Must not be NULL.
  // Returns ERROR_SUCCESS if cld_service was loaded successfully, Windows
  // error code otherwise.
  virtual DWORD Ready(CldServiceInterface* cld_service) = 0;
};

#endif  // BAR_TOOLBAR_CLD_I18N_ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOADPOLICYINTERFACE_H_
