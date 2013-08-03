// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SCOPED_REG_KEY_WIN_H_
#define REMOTING_BASE_SCOPED_REG_KEY_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/win/scoped_handle.h"

namespace remoting {

// The traits class for Win32 registry handles.
class RegKeyTraits {
 public:
  typedef HKEY Handle;

  static bool CloseHandle(HKEY handle) {
    return RegCloseKey(handle) == ERROR_SUCCESS;
  }

  static bool IsHandleValid(HKEY handle) {
    return handle != NULL;
  }

  static HKEY NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RegKeyTraits);
};

typedef base::win::GenericScopedHandle<
    RegKeyTraits, base::win::DummyVerifierTraits> ScopedRegKey;

}  // namespace remoting

#endif  // REMOTING_BASE_SCOPED_REG_KEY_WIN_H_
