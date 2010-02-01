// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/nt_internals.h"
#include "sandbox/src/sandbox_types.h"

#ifndef SANDBOX_SRC_INTERCEPTORS_64_H_
#define SANDBOX_SRC_INTERCEPTORS_64_H_

namespace sandbox {

extern "C" {

// Interception of NtMapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being loaded, so we can patch them if needed.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtMapViewOfSection64(
    HANDLE section, HANDLE process, PVOID *base, ULONG_PTR zero_bits,
    SIZE_T commit_size, PLARGE_INTEGER offset, PSIZE_T view_size,
    SECTION_INHERIT inherit, ULONG allocation_type, ULONG protect);

// Interception of NtUnmapViewOfSection on the child process.
// It should never be called directly. This function provides the means to
// detect dlls being unloaded, so we can clean up our interceptions.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtUnmapViewOfSection64(HANDLE process,
                                                               PVOID base);

// -----------------------------------------------------------------------
// Interceptors without IPC.

// Interception of NtSetInformationThread on the child process.
// It should never be called directly.
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtSetInformationThread64(
    HANDLE thread, THREAD_INFORMATION_CLASS thread_info_class,
    PVOID thread_information, ULONG thread_information_bytes);

// Interception of NtOpenThreadToken on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadToken64(
    HANDLE thread, ACCESS_MASK desired_access, BOOLEAN open_as_self,
    PHANDLE token);

// Interception of NtOpenThreadTokenEx on the child process.
// It should never be called directly
SANDBOX_INTERCEPT NTSTATUS WINAPI TargetNtOpenThreadTokenEx64(
    HANDLE thread, ACCESS_MASK desired_access, BOOLEAN open_as_self,
    ULONG handle_attributes, PHANDLE token);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTORS_64_H_
