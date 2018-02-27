// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notification_helper/notification_activator.h"

#include <shellapi.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/windows_types.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "notification_helper/trace_util.h"

namespace {

// Returns the file path of chrome.exe if found, or an empty file path if not.
base::FilePath GetChromeExePath() {
  // Look for chrome.exe one folder above notification_helper.exe (as expected
  // in Chrome installs). Failing that, look for it alonside
  // notification_helper.exe.
  base::FilePath dir_exe;
  if (!PathService::Get(base::DIR_EXE, &dir_exe))
    return base::FilePath();

  base::FilePath chrome_exe =
      dir_exe.DirName().Append(chrome::kBrowserProcessExecutableName);

  if (!base::PathExists(chrome_exe)) {
    chrome_exe = dir_exe.Append(chrome::kBrowserProcessExecutableName);
    if (!base::PathExists(chrome_exe))
      return base::FilePath();
  }
  return chrome_exe;
}

}  // namespace

namespace notification_helper {

NotificationActivator::~NotificationActivator() = default;

// Handles toast activation outside of the browser process lifecycle by
// launching chrome.exe with --notification-launch-id. This new process may
// rendezvous to an existing browser process or become a new one, as
// appropriate.
//
// When this method is called, there are three possibilities depending on the
// running state of Chrome.
// 1) NOT_RUNNING: Chrome is not running.
// 2) NEW_INSTANCE: Chrome is running, but it's NOT the same instance that sent
//    the toast.
// 3) SAME_INSTANCE : Chrome is running, and it _is_ the same instance that sent
//    the toast.
//
// Chrome could attach an activation event handler to the toast so that Windows
// can call it directly to handle the activation. However, Windows makes this
// function call only in case SAME_INSTANCE. For the other two cases, Chrome
// needs to handle the activation on its own. Since there is no way to
// differentiate cases SAME_INSTANCE and NEW_INSTANCE in this
// notification_helper process, Chrome doesn't attach an activation event
// handler to the toast and handles all three cases through the command line.
HRESULT NotificationActivator::Activate(
    LPCWSTR app_user_model_id,
    LPCWSTR invoked_args,
    const NOTIFICATION_USER_INPUT_DATA* data,
    ULONG count) {
  base::FilePath chrome_exe_path = GetChromeExePath();
  if (chrome_exe_path.empty()) {
    Trace(L"Failed to get chrome exe path\n");
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  // |invoked_args| contains the launch ID string encoded by Chrome. Chrome adds
  // it to the launch argument of the toast and gets it back via |invoked_args|
  // here. Chrome needs the data to be able to look up the notification on its
  // end.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kNotificationLaunchId,
                                  invoked_args);
  base::string16 params(command_line.GetCommandLineString());

  SHELLEXECUTEINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_LOG_USAGE;
  info.lpFile = chrome_exe_path.value().c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOWNORMAL;

  if (!::ShellExecuteEx(&info)) {
    DWORD error_code = ::GetLastError();
    Trace(L"Unable to launch Chrome.exe; error: 0x%08X\n", error_code);
    return HRESULT_FROM_WIN32(error_code);
  }

  return S_OK;
}

}  // namespace notification_helper
