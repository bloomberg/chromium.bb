// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mutex to guarantee serialization of RLZ key accesses.

#ifndef RLZ_WIN_LIB_LIB_MUTEX_H_
#define RLZ_WIN_LIB_LIB_MUTEX_H_

#include <windows.h>

namespace rlz_lib {

class LibMutex {
 public:
  LibMutex();
  ~LibMutex();

  bool failed(void) { return !acquired_; }

 private:
  bool acquired_;
  HANDLE mutex_;
};

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_LIB_MUTEX_H_
