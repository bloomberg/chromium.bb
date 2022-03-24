// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/init_logging.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "components/version_info/version_info.h"

namespace cr_fuchsia {

// These values must match content/public/common/content_switches.cc so that
// the values will be passed to child processes in projects that Chromium's
// Content layer.
constexpr char kEnableLogging[] = "enable-logging";
constexpr char kLogFile[] = "log-file";

bool InitLoggingFromCommandLine(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  if (command_line.GetSwitchValueASCII(kEnableLogging) == "stderr") {
    settings.logging_dest = logging::LOG_TO_STDERR;
  } else {
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  }
  if (command_line.HasSwitch(kLogFile)) {
    settings.logging_dest |= logging::LOG_TO_FILE;
    settings.log_file_path = command_line.GetSwitchValueASCII(kLogFile).c_str();
    settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  }
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
  return logging::InitLogging(settings);
}

bool InitLoggingFromCommandLineDefaultingToStderrForTest(
    base::CommandLine* command_line) {
  // Set logging to stderr if not specified.
  if (!command_line->HasSwitch(cr_fuchsia::kEnableLogging)) {
    command_line->AppendSwitchNative(cr_fuchsia::kEnableLogging, "stderr");
  }

  return InitLoggingFromCommandLine(*command_line);
}

void LogComponentStartWithVersion(base::StringPiece component_name) {
  std::string version_string =
      base::StringPrintf("Starting %s %s", component_name.data(),
                         version_info::GetVersionNumber().c_str());
#if !defined(OFFICIAL_BUILD)
  version_string += " (built at " + version_info::GetLastChange() + ")";
#endif  // !defined(OFFICIAL_BUILD)

  LOG(INFO) << version_string;
}

}  // namespace cr_fuchsia
