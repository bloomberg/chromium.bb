// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/crash_reporter.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace updater {
namespace {

crashpad::CrashpadClient& GetCrashpadClient() {
  static crashpad::CrashpadClient crashpad_client;
  return crashpad_client;
}

// Returns the command line arguments to start the crash handler process with.
std::vector<std::string> MakeCrashHandlerArgs(UpdaterScope updater_scope) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kCrashHandlerSwitch);
  command_line.AppendSwitch(kEnableLoggingSwitch);
  command_line.AppendSwitchASCII(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue);
  if (updater_scope == UpdaterScope::kSystem) {
    command_line.AppendSwitch(kSystemSwitch);
  }

  // The first element in the command line arguments is the program name,
  // which must be skipped.
#if defined(OS_WIN)
  std::vector<std::string> args;
  std::transform(++command_line.argv().begin(), command_line.argv().end(),
                 std::back_inserter(args),
                 [](const auto& arg) { return base::WideToUTF8(arg); });

  return args;
#else
  return {++command_line.argv().begin(), command_line.argv().end()};
#endif
}

}  // namespace

void StartCrashReporter(UpdaterScope updater_scope,
                        const std::string& version) {
  static bool started = false;
  DCHECK(!started);
  started = true;

  base::FilePath handler_path;
  base::PathService::Get(base::FILE_EXE, &handler_path);

  const absl::optional<base::FilePath> database_path =
      GetVersionedDirectory(updater_scope);
  if (!database_path) {
    LOG(DFATAL) << "Failed to get the database path.";
    return;
  }

  std::map<std::string, std::string> annotations;
  annotations["ver"] = version;
  annotations["prod"] = PRODUCT_FULLNAME_STRING;

  // TODO(crbug.com/1163583): use the production front end instead of staging.
  crashpad::CrashpadClient& client = GetCrashpadClient();
  if (!client.StartHandler(handler_path, *database_path,
                           /*metrics_dir=*/base::FilePath(),
                           CRASH_STAGING_UPLOAD_URL, annotations,
                           MakeCrashHandlerArgs(updater_scope),
                           /*restartable=*/true,
                           /*asynchronous_start=*/false)) {
    LOG(DFATAL) << "Failed to start handler.";
    return;
  }

  VLOG(1) << "Crash handler launched and ready.";
}

int CrashReporterMain() {
  base::CommandLine command_line = *base::CommandLine::ForCurrentProcess();
  DCHECK(command_line.HasSwitch(kCrashHandlerSwitch));

  // Disable rate-limiting until this is fixed:
  //   https://bugs.chromium.org/p/crashpad/issues/detail?id=23
  command_line.AppendSwitch(kNoRateLimitSwitch);

  // Because of https://bugs.chromium.org/p/crashpad/issues/detail?id=82,
  // Crashpad fails on the presence of flags it doesn't handle.
  command_line.RemoveSwitch(kCrashHandlerSwitch);
  command_line.RemoveSwitch(kSystemSwitch);
  command_line.RemoveSwitch(kEnableLoggingSwitch);
  command_line.RemoveSwitch(kLoggingModuleSwitch);

  const std::vector<base::CommandLine::StringType> argv = command_line.argv();

  // |storage| must be declared before |argv_as_utf8|, to ensure it outlives
  // |argv_as_utf8|, which will hold pointers into |storage|.
  std::vector<std::string> storage;
  auto argv_as_utf8 = std::make_unique<char*[]>(argv.size() + 1);
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
#if defined(OS_WIN)
    storage.push_back(base::WideToUTF8(argv[i]));
#else
    storage.push_back(argv[i]);
#endif
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;

  return crashpad::HandlerMain(argv.size(), argv_as_utf8.get(),
                               /*user_stream_sources=*/nullptr);
}

}  // namespace updater
