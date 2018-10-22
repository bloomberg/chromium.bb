// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace IPC {

#if defined(OS_WIN)
PlatformFileForTransit::PlatformFileForTransit() : handle_(nullptr) {}

PlatformFileForTransit::PlatformFileForTransit(HANDLE handle)
    : handle_(handle) {}

bool PlatformFileForTransit::operator==(
    const PlatformFileForTransit& platform_file) const {
  return handle_ == platform_file.handle_;
}

bool PlatformFileForTransit::operator!=(
    const PlatformFileForTransit& platform_file) const {
  return !(*this == platform_file);
}

HANDLE PlatformFileForTransit::GetHandle() const {
  return handle_;
}

bool PlatformFileForTransit::IsValid() const {
  return handle_ != nullptr;
}

#endif  // defined(OS_WIN)

PlatformFileForTransit GetPlatformFileForTransit(base::PlatformFile handle,
                                                 bool close_source_handle) {
#if defined(OS_WIN)
  HANDLE raw_handle = INVALID_HANDLE_VALUE;
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (close_source_handle)
    options |= DUPLICATE_CLOSE_SOURCE;
  if (handle == INVALID_HANDLE_VALUE ||
      !::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(),
                         &raw_handle, 0, FALSE, options)) {
    return IPC::InvalidPlatformFileForTransit();
  }

  return IPC::PlatformFileForTransit(raw_handle);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // If asked to close the source, we can simply re-use the source fd instead of
  // dup()ing and close()ing.
  // When we're not closing the source, we need to duplicate the handle and take
  // ownership of that. The reason is that this function is often used to
  // generate IPC messages, and the handle must remain valid until it's sent to
  // the other process from the I/O thread. Without the dup, calling code might
  // close the source handle before the message is sent, creating a race
  // condition.
  int fd = close_source_handle ? handle : HANDLE_EINTR(::dup(handle));
  return base::FileDescriptor(fd, true);
#endif
}

PlatformFileForTransit TakePlatformFileForTransit(base::File file) {
  return GetPlatformFileForTransit(file.TakePlatformFile(), true);
}

}  // namespace IPC
