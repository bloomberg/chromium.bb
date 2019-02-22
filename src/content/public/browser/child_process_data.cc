// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/child_process_data.h"

#if defined(OS_WIN)
#include <Windows.h>
#endif

namespace content {

#if defined(OS_WIN)
void ChildProcessData::SetHandle(base::ProcessHandle process) {
  HANDLE handle_to_set;
  if (process == base::kNullProcessHandle) {
    handle_to_set = base::kNullProcessHandle;
  } else {
    BOOL result =
        ::DuplicateHandle(::GetCurrentProcess(), process, ::GetCurrentProcess(),
                          &handle_to_set, 0, FALSE, DUPLICATE_SAME_ACCESS);
    auto err = GetLastError();
    CHECK(result) << process << " " << err;
  }
  handle_ = base::win::ScopedHandle(handle_to_set);
}
#endif

ChildProcessData::ChildProcessData(int process_type)
    : process_type(process_type), id(0), handle_(base::kNullProcessHandle) {}

ChildProcessData::ChildProcessData(ChildProcessData&& rhs)
    : process_type(rhs.process_type),
      name(rhs.name),
      metrics_name(rhs.metrics_name),
      id(rhs.id) {
#if defined(OS_WIN)
  handle_.Set(rhs.handle_.Take());
#else
  handle_ = rhs.handle_;
  rhs.handle_ = base::kNullProcessHandle;
#endif
}

ChildProcessData::~ChildProcessData() {}

ChildProcessData ChildProcessData::Duplicate() const {
  ChildProcessData result(process_type);
  result.name = name;
  result.metrics_name = metrics_name;
  result.id = id;
#if defined(OS_WIN)
  result.SetHandle(handle_.Get());
#else
  result.SetHandle(handle_);
#endif
  return result;
}

}  // namespace content
