// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/app/app_update_all.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"

#if defined(OS_WIN)
#include "chrome/updater/server/win/server.h"
#include "chrome/updater/server/win/service_main.h"
#include "chrome/updater/win/install_app.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/updater/mac/setup/install_app.h"
#include "chrome/updater/mac/setup/swap_app.h"
#include "chrome/updater/server/mac/server.h"
#endif

// Instructions For Windows.
// - To install only the updater, run "updatersetup.exe" from the build out dir.
// - To install Chrome and the updater, do the same but use the --appid:
//    updatersetup.exe --appid={8A69D345-D564-463C-AFF1-A69D9E530F96}
// - To uninstall, run "updater.exe --uninstall" from its install directory,
// which is under %LOCALAPPDATA%\Google\GoogleUpdater, or from the |out|
// directory of the build.
// - To debug, append the following arguments to any updater command line:
//    --enable-logging --vmodule=*/chrome/updater/*=2.
// - To run the `updater --install` from the `out` directory of the build,
//   use --install-from-out-dir command line switch in addition to other
//   arguments for --install.

namespace updater {

namespace {

// The log file is created in DIR_LOCAL_APP_DATA or DIR_APP_DATA.
void InitLogging(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  base::FilePath log_dir;
  GetProductDirectory(&log_dir);
  const auto log_file = log_dir.Append(FILE_PATH_LITERAL("updater.log"));
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // enable_process_id
                       true,    // enable_thread_id
                       true,    // enable_timestamp
                       false);  // enable_tickcount
  VLOG(1) << "Version " << UPDATER_VERSION_STRING << ", log file "
          << settings.log_file_path;
}

void InitializeCrashReporting() {
  crash_reporter::InitializeCrashKeys();
  static crash_reporter::CrashKeyString<16> crash_key_process_type(
      "process_type");
  crash_key_process_type.Set("updater");
  if (CrashClient::GetInstance()->InitializeCrashReporting())
    VLOG(1) << "Crash reporting initialized.";
  else
    VLOG(1) << "Crash reporting is not available.";
  StartCrashReporter(UPDATER_VERSION_STRING);
}

}  // namespace

int HandleUpdaterCommands(const base::CommandLine* command_line) {
  DCHECK(!command_line->HasSwitch(kCrashHandlerSwitch));

  if (command_line->HasSwitch(kCrashMeSwitch)) {
    // Records a backtrace in the log, crashes the program, saves a crash dump,
    // and reports the crash.
    CHECK(false) << "--crash-me was used.";
  }

  if (command_line->HasSwitch(kServerSwitch)) {
    return AppServerInstance()->Run();
  }

#if defined(OS_WIN)
  if (command_line->HasSwitch(kComServiceSwitch))
    return ServiceMain::RunComService(command_line);
#endif  // OS_WIN

  if (command_line->HasSwitch(kInstallSwitch))
    return AppInstallInstance()->Run();

#if defined(OS_MACOSX)
  if (command_line->HasSwitch(kSwapUpdaterSwitch))
    return AppSwapUpdaterInstance()->Run();
#endif  // OS_MACOSX

  if (command_line->HasSwitch(kUninstallSwitch))
    return AppUninstallInstance()->Run();

  if (command_line->HasSwitch(kUpdateAppsSwitch)) {
    return AppUpdateAllInstance()->Run();
  }

  VLOG(1) << "Unknown command line switch.";
  return -1;
}

int UpdaterMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestSwitch))
    return 0;

  InitLogging(*command_line);

  VLOG(1) << "Command line: " << command_line->GetCommandLineString();
  if (command_line->HasSwitch(kCrashHandlerSwitch))
    return CrashReporterMain();

  InitializeCrashReporting();

  return HandleUpdaterCommands(command_line);
}

}  // namespace updater
