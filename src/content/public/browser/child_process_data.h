// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_DATA_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_DATA_H_

#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace content {

// Holds information about a child process.
struct CONTENT_EXPORT ChildProcessData {
  // The type of the process. See the content::ProcessType enum for the
  // well-known process types.
  int process_type;

  // The name of the process.  i.e. for plugins it might be Flash, while for
  // for workers it might be the domain that it's from.
  base::string16 name;

  // The non-localized name of the process used for metrics reporting.
  std::string metrics_name;

  // The unique identifier for this child process. This identifier is NOT a
  // process ID, and will be unique for all types of child process for
  // one run of the browser.
  int id;

#if defined(OS_WIN)
  base::ProcessHandle GetHandle() const { return handle_.Get(); }
  // Will duplicate the handle and assume ownership of the duplicate.
  void SetHandle(base::ProcessHandle process);
#else
  base::ProcessHandle GetHandle() const { return handle_; }
  void SetHandle(base::ProcessHandle process) { handle_ = process; }
#endif

  bool IsHandleValid() const { return GetHandle() != base::kNullProcessHandle; }

  explicit ChildProcessData(int process_type);
  ~ChildProcessData();

  ChildProcessData(ChildProcessData&& rhs);

  // Copying these objects requires duplicating the handle which is moderately
  // expensive, so make it an explicit action.
  ChildProcessData Duplicate() const;

 private:
// The handle to the process. May have value kNullProcessHandle if no process
// exists - either because it hasn't been started yet or it's running in the
// current process.
#if defined(OS_WIN)
  // Must be a scoped handle on Windows holding a duplicated handle or else
  // there are no guarantees the handle will still be valid when used.
  base::win::ScopedHandle handle_;
#else
  base::ProcessHandle handle_;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_DATA_H_
