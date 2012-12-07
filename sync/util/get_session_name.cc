// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/get_session_name.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/sys_info.h"
#include "base/task_runner.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#elif defined(OS_LINUX)
#include "base/linux_util.h"
#elif defined(OS_IOS)
#include "sync/util/get_session_name_ios.h"
#elif defined(OS_MACOSX)
#include "sync/util/get_session_name_mac.h"
#elif defined(OS_WIN)
#include "sync/util/get_session_name_win.h"
#elif defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace syncer {

namespace {

std::string GetSessionNameSynchronously() {
  std::string session_name;
#if defined(OS_CHROMEOS)
  // The approach below is similar to that used by the CrOs implementation of
  // StatisticsProvider::GetMachineStatistic(CHROMEOS_RELEASE_BOARD).
  // See chrome/browser/chromeos/system/statistics_provider.{h|cc}.
  //
  // We cannot use StatisticsProvider here because of the mutual dependency
  // it creates between sync.gyp:sync and chrome.gyp:browser.
  //
  // Even though this code is ad hoc and fragile, it remains the only means of
  // determining the Chrome OS hardware platform so we can display the right
  // device name in the "Other devices" section of the new tab page.
  // TODO(rsimha): Change this once a better alternative is available.
  // See http://crbug.com/126732.
  std::string board;
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kChromeOSReleaseBoard)) {
    board = command_line->
        GetSwitchValueASCII(chromeos::switches::kChromeOSReleaseBoard);
  } else {
    LOG(ERROR) << "Failed to get board information";
  }

  // Currently, only "stumpy" type of board is considered Chromebox, and
  // anything else is Chromebook.  On these devices, session_name should look
  // like "stumpy-signed-mp-v2keys" etc. The information can be checked on
  // "CHROMEOS_RELEASE_BOARD" line in chrome://system.
  session_name = board.substr(0, 6) == "stumpy" ? "Chromebox" : "Chromebook";
#elif defined(OS_LINUX)
  session_name = base::GetLinuxDistro();
#elif defined(OS_IOS)
  session_name = internal::GetComputerName();
#elif defined(OS_MACOSX)
  session_name = internal::GetHardwareModelName();
#elif defined(OS_WIN)
  session_name = internal::GetComputerName();
#elif defined(OS_ANDROID)
  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();
  session_name = android_build_info->model();
#endif

  if (session_name == "Unknown" || session_name.empty())
    session_name = base::SysInfo::OperatingSystemName();

  return session_name;
}

void FillSessionName(std::string* session_name) {
  *session_name = GetSessionNameSynchronously();
}

void OnSessionNameFilled(
    const base::Callback<void(const std::string&)>& done_callback,
    std::string* session_name) {
  done_callback.Run(*session_name);
}

}  // namespace

void GetSessionName(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Callback<void(const std::string&)>& done_callback) {
  std::string* session_name = new std::string();
  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FillSessionName,
                 base::Unretained(session_name)),
      base::Bind(&OnSessionNameFilled,
                 done_callback,
                 base::Owned(session_name)));
}

std::string GetSessionNameSynchronouslyForTesting() {
  return GetSessionNameSynchronously();
}

}  // namespace syncer
