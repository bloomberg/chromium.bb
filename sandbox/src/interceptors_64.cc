// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/interceptors_64.h"

#include "sandbox/src/interceptors.h"
#include "sandbox/src/policy_target.h"
#include "sandbox/src/sandbox_nt_types.h"
#include "sandbox/src/sandbox_types.h"
#include "sandbox/src/target_interceptions.h"

namespace sandbox {

SANDBOX_INTERCEPT NtExports g_nt;
SANDBOX_INTERCEPT OriginalFunctions g_originals;

NTSTATUS WINAPI TargetNtMapViewOfSection64(
    HANDLE section, HANDLE process, PVOID *base, ULONG_PTR zero_bits,
    SIZE_T commit_size, PLARGE_INTEGER offset, PSIZE_T view_size,
    SECTION_INHERIT inherit, ULONG allocation_type, ULONG protect) {
  NtMapViewOfSectionFunction orig_fn = reinterpret_cast<
      NtMapViewOfSectionFunction>(g_originals[MAP_VIEW_OF_SECTION_ID]);

  return TargetNtMapViewOfSection(orig_fn, section, process, base, zero_bits,
                                  commit_size, offset, view_size, inherit,
                                  allocation_type, protect);
}

NTSTATUS WINAPI TargetNtUnmapViewOfSection64(HANDLE process, PVOID base) {
  NtUnmapViewOfSectionFunction orig_fn = reinterpret_cast<
      NtUnmapViewOfSectionFunction>(g_originals[UNMAP_VIEW_OF_SECTION_ID]);
  return TargetNtUnmapViewOfSection(orig_fn, process, base);
}

NTSTATUS WINAPI TargetNtSetInformationThread64(
    HANDLE thread, THREAD_INFORMATION_CLASS thread_info_class,
    PVOID thread_information, ULONG thread_information_bytes) {
  NtSetInformationThreadFunction orig_fn = reinterpret_cast<
      NtSetInformationThreadFunction>(g_originals[SET_INFORMATION_THREAD_ID]);
  return TargetNtSetInformationThread(orig_fn, thread, thread_info_class,
                                      thread_information,
                                      thread_information_bytes);
}

NTSTATUS WINAPI TargetNtOpenThreadToken64(
    HANDLE thread, ACCESS_MASK desired_access, BOOLEAN open_as_self,
    PHANDLE token) {
  NtOpenThreadTokenFunction orig_fn = reinterpret_cast<
      NtOpenThreadTokenFunction>(g_originals[OPEN_THREAD_TOKEN_ID]);
  return TargetNtOpenThreadToken(orig_fn, thread, desired_access, open_as_self,
                                 token);
}

NTSTATUS WINAPI TargetNtOpenThreadTokenEx64(
    HANDLE thread, ACCESS_MASK desired_access, BOOLEAN open_as_self,
    ULONG handle_attributes, PHANDLE token) {
  NtOpenThreadTokenExFunction orig_fn = reinterpret_cast<
      NtOpenThreadTokenExFunction>(g_originals[OPEN_THREAD_TOKEN_EX_ID]);
  return TargetNtOpenThreadTokenEx(orig_fn, thread, desired_access,
                                   open_as_self, handle_attributes, token);
}

}  // namespace sandbox
