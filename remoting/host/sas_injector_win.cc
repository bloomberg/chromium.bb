// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/sas_injector.h"

#include <windows.h>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "third_party/webrtc/modules/desktop_capture/win/desktop.h"
#include "third_party/webrtc/modules/desktop_capture/win/scoped_thread_desktop.h"

namespace remoting {

namespace {

// Names of the API and library implementing software SAS generation.
const base::FilePath::CharType kSasDllFileName[] = FILE_PATH_LITERAL("sas.dll");
const char kSendSasName[] = "SendSAS";

// The prototype of SendSAS().
typedef VOID (WINAPI *SendSasFunc)(BOOL);

// The registry key and value holding the policy controlling software SAS
// generation.
const wchar_t kSystemPolicyKeyName[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
const wchar_t kSoftwareSasValueName[] = L"SoftwareSASGeneration";

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
      PLOG(ERROR) << "Failed to restore the software SAS generation policy";
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
    PLOG(ERROR) << "Failed to open 'HKLM\\" << kSystemPolicyKeyName << "'";
    return false;
  }

  bool custom_policy = system_policy_.HasValue(kSoftwareSasValueName);

  // Override the default policy (i.e. there is no value in the registry) only.
  if (!custom_policy) {
    result = system_policy_.WriteValue(kSoftwareSasValueName,
                                       kEnableSoftwareSasByServices);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Failed to enable software SAS generation by services";
      return false;
    } else {
      restore_policy_ = true;
    }
  }

  return true;
}

} // namespace

// Sends Secure Attention Sequence using the SendSAS() function from sas.dll.
// This library is shipped starting from Win7/W2K8 R2 only. However Win7 SDK
// includes a redistributable verion of the same library that works on
// Vista/W2K8. We install the latter along with our binaries.
class SasInjectorWin : public SasInjector {
 public:
  SasInjectorWin();
  virtual ~SasInjectorWin();

  // SasInjector implementation.
  virtual bool InjectSas() OVERRIDE;

 private:
  base::ScopedNativeLibrary sas_dll_;
  SendSasFunc send_sas_;
};

// Emulates Secure Attention Sequence (Ctrl+Alt+Del) by switching to
// the Winlogon desktop and injecting Ctrl+Alt+Del as a hot key.
// N.B. Windows XP/W2K3 only.
class SasInjectorXp : public SasInjector {
 public:
  SasInjectorXp();
  virtual ~SasInjectorXp();

  // SasInjector implementation.
  virtual bool InjectSas() OVERRIDE;
};

SasInjectorWin::SasInjectorWin() : send_sas_(NULL) {
}

SasInjectorWin::~SasInjectorWin() {
}

bool SasInjectorWin::InjectSas() {
  // Load sas.dll. The library is expected to be in the same folder as this
  // binary.
  if (!sas_dll_.is_valid()) {
    base::FilePath dir_path;
    if (!PathService::Get(base::DIR_EXE, &dir_path)) {
      LOG(ERROR) << "Failed to get the executable file name.";
      return false;
    }

    sas_dll_.Reset(base::LoadNativeLibrary(dir_path.Append(kSasDllFileName),
                                           NULL));
  }
  if (!sas_dll_.is_valid()) {
    LOG(ERROR) << "Failed to load '" << kSasDllFileName << "'";
    return false;
  }

  // Get the pointer to sas!SendSAS().
  if (send_sas_ == NULL) {
    send_sas_ = reinterpret_cast<SendSasFunc>(
        sas_dll_.GetFunctionPointer(kSendSasName));
  }
  if (send_sas_ == NULL) {
    LOG(ERROR) << "Failed to retrieve the address of '" << kSendSasName
               << "()'";
    return false;
  }

  // Enable software SAS generation by services and send SAS. SAS can still fail
  // if the policy does not allow services to generate software SAS.
  ScopedSoftwareSasPolicy enable_sas;
  if (!enable_sas.Apply())
    return false;

  (*send_sas_)(FALSE);
  return true;
}

SasInjectorXp::SasInjectorXp() {
}

SasInjectorXp::~SasInjectorXp() {
}

bool SasInjectorXp::InjectSas() {
  const wchar_t kWinlogonDesktopName[] = L"Winlogon";
  const wchar_t kSasWindowClassName[] = L"SAS window class";
  const wchar_t kSasWindowTitle[] = L"SAS window";

  scoped_ptr<webrtc::Desktop> winlogon_desktop(
      webrtc::Desktop::GetDesktop(kWinlogonDesktopName));
  if (!winlogon_desktop.get()) {
    PLOG(ERROR) << "Failed to open '" << kWinlogonDesktopName << "' desktop";
    return false;
  }

  webrtc::ScopedThreadDesktop desktop;
  if (!desktop.SetThreadDesktop(winlogon_desktop.release())) {
    PLOG(ERROR) << "Failed to switch to '" << kWinlogonDesktopName
                << "' desktop";
    return false;
  }

  HWND window = FindWindow(kSasWindowClassName, kSasWindowTitle);
  if (!window) {
    PLOG(ERROR) << "Failed to find '" << kSasWindowTitle << "' window";
    return false;
  }

  if (PostMessage(window,
                  WM_HOTKEY,
                  0,
                  MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE)) == 0) {
    PLOG(ERROR) << "Failed to post WM_HOTKEY message";
    return false;
  }

  return true;
}

scoped_ptr<SasInjector> SasInjector::Create() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    return scoped_ptr<SasInjector>(new SasInjectorXp());
  } else {
    return scoped_ptr<SasInjector>(new SasInjectorWin());
  }
}

} // namespace remoting
