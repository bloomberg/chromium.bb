// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/evaluate_capability.h"

#include <iostream>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/switches.h"

namespace remoting {

namespace {

// TODO(zijiehe): Move these test specific handlers into test binary.
// This function is for test purpose only. It writes some random texts to both
// stdout and stderr, and returns a random value 234.
int EvaluateTest() {
  std::cout << "In EvaluateTest(): Line 1\n"
               "In EvaluateTest(): Line 2";
  std::cerr << "In EvaluateTest(): Error Line 1\n"
               "In EvaluateTest(): Error Line 2";
  return 234;
}

// This function is for test purpose only. It triggers an assertion failure.
int EvaluateCrash() {
  NOTREACHED();
  return 0;
}

// This function is for test purpose only. It forwards the evaluation request to
// a new process and returns what it returns.
int EvaluateForward() {
  std::string output;
  int result = EvaluateCapability(kEvaluateTest, &output);
  std::cout << output;
  return result;
}

// Returns the full path of the binary file we should use to evaluate the
// capability. According to the platform and executing environment, return of
// this function may vary. But in one process, the return value is guaranteed to
// be the same.
// This function tries to use current binary if supported, otherwise it falls
// back to use the default binary.
base::FilePath BuildHostBinaryPath() {
#if defined(OS_LINUX)
  base::FilePath path;
  bool result = base::PathService::Get(base::FILE_EXE, &path);
  DCHECK(result);
  if (path.BaseName().value() ==
      FILE_PATH_LITERAL("chrome-remote-desktop-host")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_me2me_host")) {
    return path;
  }

  result = base::PathService::Get(base::DIR_EXE, &path);
  DCHECK(result);
  return path.Append(FILE_PATH_LITERAL("remoting_me2me_host"));
#elif defined(OS_MACOSX)
  base::FilePath path;
  bool result = base::PathService::Get(base::FILE_EXE, &path);
  DCHECK(result);
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_me2me_host")) {
    return path;
  }

  result = base::PathService::Get(base::DIR_EXE, &path);
  DCHECK(result);
  return path.Append(FILE_PATH_LITERAL(
      "remoting_me2me_host.app/Contents/MacOS/remoting_me2me_host"));
#elif defined(OS_WIN)
  base::FilePath path;
  bool result = base::PathService::Get(base::FILE_EXE, &path);
  DCHECK(result);
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_console.exe")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_desktop.exe")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_host.exe")) {
    return path;
  }

  result = base::PathService::Get(base::DIR_EXE, &path);
  DCHECK(result);
  return path.Append(FILE_PATH_LITERAL("remoting_host.exe"));
#else
  #error "BuildHostBinaryPath is not implemented for current platform."
#endif
}

}  // namespace

int EvaluateCapabilityLocally(const std::string& type) {
  // TODO(zijiehe): Move these test specific handlers into test binary.
  if (type == kEvaluateTest) {
    return EvaluateTest();
  }
  if (type == kEvaluateCrash) {
    return EvaluateCrash();
  }
  if (type == kEvaluateForward) {
    return EvaluateForward();
  }

  return kInvalidCommandLineExitCode;
}

int EvaluateCapability(const std::string& type,
                       std::string* output /* = nullptr */) {
  base::CommandLine command(BuildHostBinaryPath());
  command.AppendSwitchASCII(kProcessTypeSwitchName,
                            kProcessTypeEvaluateCapability);
  command.AppendSwitchASCII(kEvaluateCapabilitySwitchName, type);

  int exit_code;
  std::string dummy_output;
  if (!output) {
    output = &dummy_output;
  }

  // base::GetAppOutputWithExitCode() usually returns false when receiving
  // "unknown" exit code. But we forward the |exit_code| through return value,
  // so the return of base::GetAppOutputWithExitCode() should be ignored.
  base::GetAppOutputWithExitCode(command, output, &exit_code);
  return exit_code;
}

}  // namespace remoting
