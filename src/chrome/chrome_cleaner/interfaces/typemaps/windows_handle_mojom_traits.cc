// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/interfaces/typemaps/windows_handle_mojom_traits.h"

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

using chrome_cleaner::mojom::SpecialWindowsHandle;
using chrome_cleaner::mojom::WindowsHandleDataView;

namespace {

bool ToSpecialHandle(HANDLE handle, SpecialWindowsHandle* out_special_handle) {
  DCHECK(out_special_handle);

  if (handle == nullptr) {
    *out_special_handle = SpecialWindowsHandle::NULL_HANDLE;
    return true;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    *out_special_handle = SpecialWindowsHandle::INVALID_HANDLE;
    return true;
  }
  if (handle == HKEY_CLASSES_ROOT) {
    *out_special_handle = SpecialWindowsHandle::CLASSES_ROOT;
    return true;
  }
  if (handle == HKEY_CURRENT_CONFIG) {
    *out_special_handle = SpecialWindowsHandle::CURRENT_CONFIG;
    return true;
  }
  if (handle == HKEY_CURRENT_USER) {
    *out_special_handle = SpecialWindowsHandle::CURRENT_USER;
    return true;
  }
  if (handle == HKEY_LOCAL_MACHINE) {
    *out_special_handle = SpecialWindowsHandle::LOCAL_MACHINE;
    return true;
  }
  if (handle == HKEY_USERS) {
    *out_special_handle = SpecialWindowsHandle::USERS;
    return true;
  }
  return false;
}

bool IsSpecialHandle(HANDLE handle) {
  SpecialWindowsHandle unused;
  return ToSpecialHandle(handle, &unused);
}

bool FromSpecialHandle(SpecialWindowsHandle special_handle,
                       HANDLE* out_handle) {
  DCHECK(out_handle);

  switch (special_handle) {
    case SpecialWindowsHandle::NULL_HANDLE:
      *out_handle = nullptr;
      return true;

    case SpecialWindowsHandle::INVALID_HANDLE:
      *out_handle = INVALID_HANDLE_VALUE;
      return true;

    case SpecialWindowsHandle::CLASSES_ROOT:
      *out_handle = HKEY_CLASSES_ROOT;
      return true;

    case SpecialWindowsHandle::CURRENT_CONFIG:
      *out_handle = HKEY_CURRENT_CONFIG;
      return true;

    case SpecialWindowsHandle::CURRENT_USER:
      *out_handle = HKEY_CURRENT_USER;
      return true;

    case SpecialWindowsHandle::LOCAL_MACHINE:
      *out_handle = HKEY_LOCAL_MACHINE;
      return true;

    case SpecialWindowsHandle::USERS:
      *out_handle = HKEY_USERS;
      return true;

    default:
      return false;
  }
}

// Duplicates a handle in the current process. Returns INVALID_HANDLE_VALUE on
// error.
HANDLE DuplicateWindowsHandle(HANDLE source_handle) {
  const HANDLE current_process = ::GetCurrentProcess();
  HANDLE new_handle = INVALID_HANDLE_VALUE;
  if (::DuplicateHandle(current_process, source_handle, current_process,
                        &new_handle, 0, FALSE, DUPLICATE_SAME_ACCESS) == 0) {
    PLOG(ERROR) << "Error duplicating handle " << source_handle;
    return INVALID_HANDLE_VALUE;
  }
  return new_handle;
}

}  // namespace

// static
mojo::ScopedHandle UnionTraits<WindowsHandleDataView, HANDLE>::raw_handle(
    HANDLE handle) {
  DCHECK_EQ(WindowsHandleDataView::Tag::RAW_HANDLE, GetTag(handle));

  if (IsSpecialHandle(handle)) {
    CHECK(false) << "Accessor raw_handle() should only be called when the "
                    "union's tag is RAW_HANDLE.";
    return mojo::ScopedHandle();
  }

  HANDLE duplicate_handle = DuplicateWindowsHandle(handle);
  return WrapPlatformFile(duplicate_handle);
}

// static
SpecialWindowsHandle UnionTraits<WindowsHandleDataView, HANDLE>::special_handle(
    HANDLE handle) {
  DCHECK_EQ(WindowsHandleDataView::Tag::SPECIAL_HANDLE, GetTag(handle));

  SpecialWindowsHandle special_handle;
  if (ToSpecialHandle(handle, &special_handle))
    return special_handle;

  CHECK(false) << "Accessor special_handle() should only be called when the "
                  "union's tag is SPECIAL_HANDLE.";
  return SpecialWindowsHandle::INVALID_HANDLE;
}

// static
WindowsHandleDataView::Tag UnionTraits<WindowsHandleDataView, HANDLE>::GetTag(
    HANDLE handle) {
  return IsSpecialHandle(handle) ? WindowsHandleDataView::Tag::SPECIAL_HANDLE
                                 : WindowsHandleDataView::Tag::RAW_HANDLE;
}

// static
bool UnionTraits<WindowsHandleDataView, HANDLE>::Read(
    WindowsHandleDataView windows_handle_view,
    HANDLE* out) {
  if (windows_handle_view.is_raw_handle()) {
    HANDLE handle;
    MojoResult mojo_result =
        UnwrapPlatformFile(windows_handle_view.TakeRawHandle(), &handle);
    if (mojo_result != MOJO_RESULT_OK) {
      *out = INVALID_HANDLE_VALUE;
      return false;
    }
    *out = handle;
    return true;
  }

  HANDLE special_handle;
  if (FromSpecialHandle(windows_handle_view.special_handle(),
                        &special_handle)) {
    *out = special_handle;
    return true;
  }

  return false;
}

}  // namespace mojo
