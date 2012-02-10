// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_
#define REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace remoting {

// Used so we always remember to close service control manager handles.
// The class interface matches that of base::win::ScopedHandle.
class ScopedScHandle {
 public:
  ScopedScHandle();
  explicit ScopedScHandle(SC_HANDLE h);
  ~ScopedScHandle();

  SC_HANDLE Get() {
    return handle_;
  }

  operator SC_HANDLE() {
    return handle_;
  }

  void Close();
  bool IsValid() const;
  void Set(SC_HANDLE new_handle);
  SC_HANDLE Take();

 private:
  SC_HANDLE handle_;
  DISALLOW_COPY_AND_ASSIGN(ScopedScHandle);
};

}  // namespace remoting

#endif  // REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_
