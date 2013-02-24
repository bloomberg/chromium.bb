// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/test/metro_registration_helper.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/win/scoped_handle.h"
#include "win8/test/test_registrar_constants.h"

namespace {

const int kRegistrationTimeoutSeconds = 30;

// Copied from util_constants.cc to avoid taking a dependency on installer_util.
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kRegistrar[] = L"test_registrar.exe";

}

namespace win8 {

bool RegisterTestDefaultBrowser(const string16& app_user_model_id,
                                const string16& viewer_process_name) {
  base::FilePath dir;
  if (!PathService::Get(base::DIR_EXE, &dir))
    return false;

  base::FilePath chrome_exe(dir.Append(kChromeExe));
  base::FilePath registrar(dir.Append(kRegistrar));

  if (!file_util::PathExists(chrome_exe) || !file_util::PathExists(registrar)) {
    LOG(ERROR) << "Could not locate " << kChromeExe << " or " << kRegistrar;
    return false;
  }

  // Perform the registration by invoking test_registrar.exe with the
  // necessary flags:
  //     test_registrar.exe /RegServer --appid=<appid> --exe-name=<name>
  CommandLine register_command(registrar);
  register_command.AppendArg("/RegServer");
  register_command.AppendSwitchNative(win8::test::kTestAppUserModelId,
                                      app_user_model_id);
  register_command.AppendSwitchNative(win8::test::kTestExeName,
                                      viewer_process_name);

  base::win::ScopedHandle register_handle;
  if (base::LaunchProcess(register_command, base::LaunchOptions(),
                          register_handle.Receive())) {
    int ret = 0;
    if (base::WaitForExitCodeWithTimeout(
            register_handle, &ret,
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

}  // namespace win8
