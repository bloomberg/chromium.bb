// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PROCESS_INSPECTOR_WIN_H_
#define CHROME_TEST_BASE_PROCESS_INSPECTOR_WIN_H_

#include <windows.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace base {
class Process;
}

// An inspector that can read properties of a remote process.
class ProcessInspector {
 public:
  // Returns an instance that reads data from |process|, which must have been
  // opened with at least PROCESS_VM_READ access rights. Returns null in case of
  // any error.
  static std::unique_ptr<ProcessInspector> Create(const base::Process& process);

  virtual ~ProcessInspector() = default;

  // Returns the parent process PID of the process.
  virtual DWORD GetParentPid() const = 0;

  // Returns the command line of the process.
  virtual const base::string16& command_line() const = 0;

 protected:
  ProcessInspector() = default;

 private:
  // Inspects |process|, returning true if all inspections succeed.
  virtual bool Inspect(const base::Process& process) = 0;

  DISALLOW_COPY_AND_ASSIGN(ProcessInspector);
};

#endif  // CHROME_TEST_BASE_PROCESS_INSPECTOR_WIN_H_
