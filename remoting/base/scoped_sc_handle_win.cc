// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/scoped_sc_handle_win.h"

namespace remoting {

ScopedScHandle::ScopedScHandle() : handle_(NULL) {
}

ScopedScHandle::ScopedScHandle(SC_HANDLE h) : handle_(NULL) {
  Set(h);
}

ScopedScHandle::~ScopedScHandle() {
  Close();
}

void ScopedScHandle::Close() {
  if (handle_) {
    if (!::CloseServiceHandle(handle_)) {
      NOTREACHED();
    }
    handle_ = NULL;
  }
}

bool ScopedScHandle::IsValid() const {
  return handle_ != NULL;
}

void ScopedScHandle::Set(SC_HANDLE new_handle) {
  Close();
  handle_ = new_handle;
}

SC_HANDLE ScopedScHandle::Take() {
  // transfers ownership away from this object
  SC_HANDLE h = handle_;
  handle_ = NULL;
  return h;
}

}  // namespace remoting
