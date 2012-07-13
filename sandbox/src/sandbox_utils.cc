// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/sandbox_utils.h"

#include <windows.h>

#include "base/logging.h"
#include "base/win/windows_version.h"
#include "sandbox/src/internal_types.h"
#include "sandbox/src/nt_internals.h"

namespace sandbox {

bool GetModuleHandleHelper(DWORD flags, const wchar_t* module_name,
                           HMODULE* module) {
  DCHECK(module);

  HMODULE kernel32_base = ::GetModuleHandle(kKerneldllName);
  if (!kernel32_base) {
    NOTREACHED();
    return false;
  }

  GetModuleHandleExFunction get_module_handle_ex = reinterpret_cast<
      GetModuleHandleExFunction>(::GetProcAddress(kernel32_base,
                                                  "GetModuleHandleExW"));
  if (get_module_handle_ex) {
    BOOL ret = get_module_handle_ex(flags, module_name, module);
    return (ret ? true : false);
  }

  if (!flags) {
    *module = ::LoadLibrary(module_name);
  } else if (flags & GET_MODULE_HANDLE_EX_FLAG_PIN) {
    NOTREACHED();
    return false;
  } else if (!(flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) {
    DCHECK((flags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) ==
           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT);

    *module = ::GetModuleHandle(module_name);
  } else {
    DCHECK((flags & (GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)) ==
           (GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS));

    MEMORY_BASIC_INFORMATION info = {0};
    size_t returned = VirtualQuery(module_name, &info, sizeof(info));
    if (sizeof(info) != returned)
      return false;
    *module = reinterpret_cast<HMODULE>(info.AllocationBase);
  }
  return true;
}

bool IsXPSP2OrLater() {
  base::win::Version version = base::win::GetVersion();
  return (version > base::win::VERSION_XP) ||
      ((version == base::win::VERSION_XP) &&
       (base::win::OSInfo::GetInstance()->service_pack().major >= 2));
}

void InitObjectAttribs(const std::wstring& name, ULONG attributes, HANDLE root,
                       OBJECT_ATTRIBUTES* obj_attr, UNICODE_STRING* uni_name) {
  static RtlInitUnicodeStringFunction RtlInitUnicodeString;
  if (!RtlInitUnicodeString) {
    HMODULE ntdll = ::GetModuleHandle(kNtdllName);
    RtlInitUnicodeString = reinterpret_cast<RtlInitUnicodeStringFunction>(
      GetProcAddress(ntdll, "RtlInitUnicodeString"));
    DCHECK(RtlInitUnicodeString);
  }
  RtlInitUnicodeString(uni_name, name.c_str());
  InitializeObjectAttributes(obj_attr, uni_name, attributes, root, NULL);
}

};  // namespace sandbox
