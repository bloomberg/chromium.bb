// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_SCOPED_PROCESS_PROTECTOR_H_
#define CHROME_CHROME_CLEANER_TEST_SCOPED_PROCESS_PROTECTOR_H_

#include <windows.h>

#include "base/win/scoped_handle.h"

namespace chrome_cleaner {

// Used to prevent a process from being interacted with in any way except via
// taking ownership and resetting the dacl. Used by tests that want unkillable
// processes.
// This protection is defeated by any process that has SeDebugPrivilege (like
// a debugging process), since that allows the process to get the ALL_ACCESS
// handle.
class ScopedProcessProtector {
 public:
  explicit ScopedProcessProtector(uint32_t process_id);
  ~ScopedProcessProtector();

  bool Initialized() { return initialized_; }

  void Release();

 private:
  void Protect(uint32_t process_id);

  base::win::ScopedHandle process_handle_;
  bool initialized_ = false;
  PACL original_dacl_ = nullptr;
  PSECURITY_DESCRIPTOR original_descriptor_ = nullptr;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_SCOPED_PROCESS_PROTECTOR_H_
