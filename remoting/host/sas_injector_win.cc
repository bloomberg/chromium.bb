// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/sas_injector.h"

#include <windows.h>
#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/stringize_macros.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"

namespace remoting {

namespace {

// Names of the API and library implementing software SAS generation.
const FilePath::CharType kSasDllFileName[] =
    FILE_PATH_LITERAL("sas.dll");
const char kSendSasName[] = "SendSAS";

// The prototype of SendSAS().
typedef VOID (WINAPI *SendSasFunc)(BOOL);

// The registry key and value holding the policy controlling software SAS
// generation.
const char16 kSystemPolicyKeyName[] =
    TO_L_STRING("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\")
    TO_L_STRING("System");
const char16 kSoftwareSasValueName[] = TO_L_STRING("SoftwareSASGeneration");

const DWORD kEnableSoftwareSasByServices = 1;

// Toggles the default software SAS generation policy to enable SAS generation
// by services. Non-default policy is not changed.
class ScopedSoftwareSasPolicy {
 public:
  ScopedSoftwareSasPolicy();
  ~ScopedSoftwareSasPolicy();

  bool Apply();

 private:
  // The handle of the registry key were SoftwareSASGeneration policy is stored.
  base::win::RegKey system_policy_;

  // True if the policy needs to be restored.
  bool restore_policy_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSoftwareSasPolicy);
};

ScopedSoftwareSasPolicy::ScopedSoftwareSasPolicy()
    : restore_policy_(false) {
}

ScopedSoftwareSasPolicy::~ScopedSoftwareSasPolicy() {
  // Restore the default policy by deleting the value that we have set.
  if (restore_policy_) {
    LONG result = system_policy_.DeleteValue(kSoftwareSasValueName);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      LOG_GETLASTERROR(ERROR)
          << "Failed to restore the software SAS generation policy";
    }
  }
}

bool ScopedSoftwareSasPolicy::Apply() {
  // Query the currently set SoftwareSASGeneration policy.
  LONG result = system_policy_.Open(HKEY_LOCAL_MACHINE,
                                    kSystemPolicyKeyName,
                                    KEY_QUERY_VALUE | KEY_SET_VALUE |
                                        KEY_WOW64_64KEY);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    LOG_GETLASTERROR(ERROR) << "Failed to open 'HKLM\\"
                            << kSystemPolicyKeyName << "'";
    return false;
  }

  bool custom_policy = system_policy_.HasValue(kSoftwareSasValueName);

  // Override the default policy (i.e. there is no value in the registry) only.
  if (!custom_policy) {
    result = system_policy_.WriteValue(kSoftwareSasValueName,
                                       kEnableSoftwareSasByServices);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      LOG_GETLASTERROR(ERROR)
          << "Failed to enable software SAS generation by services";
      return false;
    } else {
      restore_policy_ = true;
    }
  }

  return true;
}

} // namespace

// Sends the Secure Attention Sequence using the SendSAS() function from
// sas.dll. This library is shipped starting from Win7/W2K8 R2 only. However
// Win7 SDK includes a redistributable verion of the same library that works on
// Vista/W2K8. We install the latter along with our binaries.
class SasInjectorWin : public SasInjector {
 public:
  SasInjectorWin();
  virtual ~SasInjectorWin();

  // SasInjector implementation.
  virtual bool InjectSas() OVERRIDE;

 private:
  base::NativeLibrary sas_dll_;
  SendSasFunc send_sas_;
};

SasInjectorWin::SasInjectorWin() : sas_dll_(NULL), send_sas_(NULL) {
}

SasInjectorWin::~SasInjectorWin() {
  if (sas_dll_ != NULL) {
    base::UnloadNativeLibrary(sas_dll_);
  }
}

bool SasInjectorWin::InjectSas() {
  // Load sas.dll. The library is expected to be in the same folder as this
  // binary.
  if (sas_dll_ == NULL) {
    FilePath exe_path;
    if (!PathService::Get(base::FILE_EXE, &exe_path)) {
      LOG(ERROR) << "Failed to get the executable file name.";
      return false;
    }

    std::string error;
    sas_dll_ = base::LoadNativeLibrary(
                   exe_path.DirName().Append(kSasDllFileName),
                   &error);
    if (sas_dll_ == NULL) {
      LOG(ERROR) << "Failed to load '" << kSasDllFileName << "'";
      return false;
    }
  }

  // Get the pointer to sas!SendSAS().
  if (send_sas_ == NULL) {
    send_sas_ = reinterpret_cast<SendSasFunc>(
      base::GetFunctionPointerFromNativeLibrary(sas_dll_, kSendSasName));
    if (send_sas_ == NULL) {
      LOG(ERROR) << "Failed to retrieve the address of '" << kSendSasName
                 << "()'";
      return false;
    }
  }

  // Enable software SAS generation by services and send SAS. SAS can still fail
  // if the policy does not allow services to generate software SAS.
  ScopedSoftwareSasPolicy enable_sas;
  if (!enable_sas.Apply())
    return false;

  (*send_sas_)(FALSE);
  return true;
}

scoped_ptr<SasInjector> SasInjector::Create() {
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    return scoped_ptr<SasInjector>(new SasInjectorWin());
  }

  return scoped_ptr<SasInjector>();
}

} // namespace remoting
