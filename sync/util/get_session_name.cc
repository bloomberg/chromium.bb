// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/get_session_name.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task_runner.h"

#if defined(OS_LINUX)
#include "sync/util/get_session_name_linux.h"
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
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  // Currently, only "stumpy" type of board is considered Chromebox, and
  // anything else is Chromebook.  On these devices, session_name should look
  // like "stumpy-signed-mp-v2keys" etc. The information can be checked on
  // "CHROMEOS_RELEASE_BOARD" line in chrome://system.
  session_name = board.substr(0, 6) == "stumpy" ? "Chromebox" : "Chromebook";
#elif defined(OS_LINUX)
  session_name = internal::GetHostname();
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

  DCHECK(base::IsStringUTF8(session_name));
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
