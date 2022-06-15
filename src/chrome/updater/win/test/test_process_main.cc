// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/test/test_initializer.h"
#include "chrome/updater/win/test/test_strings.h"

namespace {

base::WaitableEvent EventForSwitch(const base::CommandLine& command_line,
                                   const char switch_value[]) {
  DCHECK(command_line.HasSwitch(switch_value));

  const std::wstring event_name =
      command_line.GetSwitchValueNative(switch_value);
  VLOG(1) << __func__ << " event name '" << event_name << "'";
  base::win::ScopedHandle handle(
      ::OpenEvent(EVENT_ALL_ACCESS, TRUE, event_name.c_str()));
  PLOG_IF(ERROR, !handle.IsValid())
      << __func__ << " cannot open event '" << event_name << "'";
  return base::WaitableEvent(std::move(handle));
}

}  // namespace

int main(int, char**) {
  bool success = base::CommandLine::Init(0, nullptr);
  DCHECK(success);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(updater::kEnableLoggingSwitch)) {
    InitLogging(command_line->HasSwitch(updater::kSystemSwitch)
                    ? updater::UpdaterScope::kSystem
                    : updater::UpdaterScope::kUser);
  }

  updater::NotifyInitializationDoneForTesting();

  if (command_line->HasSwitch(updater::kTestSleepMinutesSwitch)) {
    std::string value =
        command_line->GetSwitchValueASCII(updater::kTestSleepMinutesSwitch);
    int sleep_minutes = 0;
    if (base::StringToInt(value, &sleep_minutes) && sleep_minutes > 0) {
      VLOG(1) << "Process is sleeping for " << sleep_minutes << " minutes";
      ::Sleep(base::Minutes(sleep_minutes).InMilliseconds());
    } else {
      LOG(ERROR) << "Invalid sleep delay value " << value;
    }
    NOTREACHED();
    return 1;
  }

  if (command_line->HasSwitch(updater::kTestEventToSignal)) {
    EventForSwitch(*command_line, updater::kTestEventToSignal).Signal();
  } else if (command_line->HasSwitch(updater::kTestEventToWaitOn)) {
    EventForSwitch(*command_line, updater::kTestEventToWaitOn).Wait();
  }

  if (command_line->HasSwitch(updater::kTestExitCode)) {
    int exit_code = 0;
    CHECK(base::StringToInt(
        command_line->GetSwitchValueASCII(updater::kTestExitCode), &exit_code));
    VLOG(1) << "Process ending with exit code: " << exit_code;
    return exit_code;
  }

  VLOG(1) << "Process ended.";
  return 0;
}
