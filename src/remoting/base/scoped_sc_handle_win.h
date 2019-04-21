// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_
#define REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_

#include <windows.h>

#include "base/macros.h"
#include "base/win/scoped_handle.h"

namespace remoting {

class ScHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  // Closes the handle.
  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(SC_HANDLE handle) {
    return handle != NULL;
  }

  // Returns NULL handle value.
  static SC_HANDLE NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScHandleTraits);
};

typedef base::win::GenericScopedHandle<
    ScHandleTraits, base::win::DummyVerifierTraits> ScopedScHandle;

}  // namespace remoting

#endif  // REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_
