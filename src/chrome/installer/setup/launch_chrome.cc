// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/launch_chrome.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

bool LaunchChromeBrowser(const base::FilePath& application_path) {
  if (application_path.empty())
    return false;

  const base::CommandLine cmd(application_path.Append(kChromeExe));
  return base::LaunchProcess(cmd, base::LaunchOptions()).IsValid();
}

bool LaunchChromeAndWait(const base::FilePath& application_path,
                         const base::CommandLine& options,
                         int32_t* exit_code) {
  if (application_path.empty())
    return false;

  base::CommandLine cmd(application_path.Append(kChromeExe));
  cmd.AppendArguments(options, false);

  base::Process chrome_handle = base::LaunchProcess(cmd, base::LaunchOptions());
  if (!chrome_handle.IsValid()) {
    PLOG(ERROR) << "Failed to launch: " << cmd.GetCommandLineString();
    return false;
  }

  int ret = STILL_ACTIVE;
  if (!chrome_handle.WaitForExit(&ret)) {
    PLOG(ERROR) << "Wait for process exit failed";
    return false;
  }
  DCHECK_NE(ret, static_cast<int>(STILL_ACTIVE));

  if (exit_code)
    *exit_code = ret;

  return true;
}

}  // namespace installer
