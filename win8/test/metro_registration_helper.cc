// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/test/metro_registration_helper.h"

#include <shlobj.h>

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "win8/test/open_with_dialog_controller.h"
#include "win8/test/test_registrar_constants.h"

namespace {

const int kRegistrationTimeoutSeconds = 30;

// Copied from util_constants.cc to avoid taking a dependency on installer_util.
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kRegistrar[] = L"test_registrar.exe";

// Registers chrome.exe as a potential Win8 default browser.  It will then show
// up in the default browser selection dialog as kDefaultTestExeName. Intended
// to be used by a test binary in the build output directory and assumes the
// presence of test_registrar.exe, a viewer process, and all needed DLLs in the
// same directory as the calling module.
bool RegisterTestDefaultBrowser() {
  base::FilePath dir;
  if (!PathService::Get(base::DIR_EXE, &dir))
    return false;

  base::FilePath chrome_exe(dir.Append(kChromeExe));
  base::FilePath registrar(dir.Append(kRegistrar));

  if (!base::PathExists(chrome_exe) || !base::PathExists(registrar)) {
    LOG(ERROR) << "Could not locate " << kChromeExe << " or " << kRegistrar;
    return false;
  }

  // Perform the registration by invoking test_registrar.exe.
  CommandLine register_command(registrar);
  register_command.AppendArg("/RegServer");

  base::win::ScopedHandle register_handle;
  if (base::LaunchProcess(register_command.GetCommandLineString(),
                          base::LaunchOptions(),
                          &register_handle)) {
    int ret = 0;
    if (base::WaitForExitCodeWithTimeout(
            register_handle.Get(), &ret,
            base::TimeDelta::FromSeconds(kRegistrationTimeoutSeconds))) {
      if (ret == 0) {
        return true;
      } else {
        LOG(ERROR) << "Win8 registration using "
                   << register_command.GetCommandLineString()
                   << " failed with error code " << ret;
      }
    } else {
      LOG(ERROR) << "Win8 registration using "
                 << register_command.GetCommandLineString() << " timed out.";
    }
  }

  PLOG(ERROR) << "Failed to launch Win8 registration utility using "
              << register_command.GetCommandLineString();
  return false;
}

// Returns true if the test viewer's progid is the default handler for
// |protocol|.
bool IsTestDefaultForProtocol(const wchar_t* protocol) {
  base::win::ScopedComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr = registration.CreateInstance(
      CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC);
  if (FAILED(hr)) {
    LOG(ERROR) << std::hex << hr;
    return false;
  }

  base::win::ScopedCoMem<wchar_t> current_app;
  hr = registration->QueryCurrentDefault(protocol, AT_URLPROTOCOL,
                                         AL_EFFECTIVE, &current_app);
  if (FAILED(hr)) {
    LOG(ERROR) << std::hex << hr;
    return false;
  }

  return !base::string16(win8::test::kDefaultTestProgId).compare(current_app);
}

}  // namespace

namespace win8 {

bool MakeTestDefaultBrowserSynchronously() {
  static const wchar_t kDefaultBrowserProtocol[] = L"http";

  if (!RegisterTestDefaultBrowser())
    return false;

  // Make sure the registration changes have been acknowledged by the shell
  // before querying for the current default.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, NULL, NULL);

  // OpenWithDialogController will fail if the Test Runner is already default
  // since it will not show up verbatim in the dialog (e.g., in EN-US, it will
  // be prefixed by "Keep using ").
  if (IsTestDefaultForProtocol(kDefaultBrowserProtocol))
    return true;

  std::vector<base::string16> choices;
  OpenWithDialogController controller;
  HRESULT hr = controller.RunSynchronously(
      NULL, kDefaultBrowserProtocol, win8::test::kDefaultTestExeName, &choices);
  LOG_IF(ERROR, FAILED(hr)) << std::hex << hr;
  DCHECK(IsTestDefaultForProtocol(kDefaultBrowserProtocol));
  return SUCCEEDED(hr);
}

}  // namespace win8
