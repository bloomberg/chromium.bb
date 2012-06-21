// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mutex to guarantee serialization of RLZ key accesses.

#include "rlz/win/lib/lib_mutex.h"

#include <windows.h>
#include <Sddl.h>    // For SDDL_REVISION_1, ConvertStringSecurityDescript..
#include <Aclapi.h>  // For SetSecurityInfo

#include "base/logging.h"
#include "base/win/windows_version.h"

namespace {

const wchar_t kMutexName[] = L"{A946A6A9-917E-4949-B9BC-6BADA8C7FD63}";

}  // namespace anonymous

namespace rlz_lib {

// Needed to allow synchronization across integrity levels.
static bool SetObjectToLowIntegrity(HANDLE object,
    SE_OBJECT_TYPE type = SE_KERNEL_OBJECT) {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return true;  // Not needed on XP.

  // The LABEL_SECURITY_INFORMATION SDDL SACL to be set for low integrity.
  static const wchar_t kLowIntegritySddlSacl[] = L"S:(ML;;NW;;;LW)";

  bool result = false;
  DWORD error = ERROR_SUCCESS;
  PSECURITY_DESCRIPTOR security_descriptor = NULL;
  PACL sacl = NULL;
  BOOL sacl_present = FALSE;
  BOOL sacl_defaulted = FALSE;

  if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
          kLowIntegritySddlSacl, SDDL_REVISION_1, &security_descriptor, NULL)) {
    if (GetSecurityDescriptorSacl(security_descriptor, &sacl_present,
            &sacl, &sacl_defaulted)) {
      error = SetSecurityInfo(object, type, LABEL_SECURITY_INFORMATION,
                              NULL, NULL, NULL, sacl);
      result = (ERROR_SUCCESS == error);
    }
    LocalFree(security_descriptor);
  }

  return result;
}

LibMutex::LibMutex() : acquired_(false), mutex_(NULL) {
  mutex_ = CreateMutex(NULL, false, kMutexName);
  bool result = SetObjectToLowIntegrity(mutex_);
  if (result) {
    acquired_ = (WAIT_OBJECT_0 == WaitForSingleObject(mutex_, 5000L));
  }
}

LibMutex::~LibMutex() {
  if (acquired_) ReleaseMutex(mutex_);
  CloseHandle(mutex_);
}

}  // namespace rlz_lib
