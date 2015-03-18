// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_linux.h"

#include <unistd.h>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_util.h"
#include "remoting/host/host_config.h"
#include "remoting/host/usage_stats_consent.h"

namespace remoting {

namespace {

const char kDaemonScript[] =
    "/opt/google/chrome-remote-desktop/chrome-remote-desktop";

// Timeout for running daemon script. The script itself sets a timeout when
// waiting for the host to come online, so the setting here should be at least
// as long.
const int64 kDaemonTimeoutMs = 60000;

// Timeout for commands that require password prompt - 5 minutes.
const int64 kSudoTimeoutSeconds = 5 * 60;

std::string GetMd5(const std::string& value) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, value);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return base::StringToLowerASCII(base::HexEncode(digest.a, sizeof(digest.a)));
}

base::FilePath GetConfigPath() {
  std::string filename = "host#" + GetMd5(net::GetHostName()) + ".json";
  base::FilePath homedir;
  PathService::Get(base::DIR_HOME, &homedir);
  return homedir.Append(".config/chrome-remote-desktop").Append(filename);
}

bool GetScriptPath(base::FilePath* result) {
  base::FilePath candidate_exe(kDaemonScript);
  if (access(candidate_exe.value().c_str(), X_OK) == 0) {
    *result = candidate_exe;
    return true;
  }
  return false;
}

bool RunHostScriptWithTimeout(
    const std::vector<std::string>& args,
    base::TimeDelta timeout,
    int* exit_code) {
  DCHECK(exit_code);

  // As long as we're relying on running an external binary from the
  // PATH, don't do it as root.
  if (getuid() == 0) {
    LOG(ERROR) << "Refusing to run script as root.";
    return false;
  }
  base::FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    LOG(ERROR) << "GetScriptPath() failed.";
    return false;
  }
  base::CommandLine command_line(script_path);
  for (unsigned int i = 0; i < args.size(); ++i) {
    command_line.AppendArg(args[i]);
  }

  // Redirect the child's stdout to the parent's stderr. In the case where this
  // parent process is a Native Messaging host, its stdout is used to send
  // messages to the web-app.
  base::FileHandleMappingVector fds_to_remap;
  fds_to_remap.push_back(std::pair<int, int>(STDERR_FILENO, STDOUT_FILENO));
  base::LaunchOptions options;
  options.fds_to_remap = &fds_to_remap;

#if !defined(OS_CHROMEOS)
  options.allow_new_privs = true;
#endif

  base::Process process = base::LaunchProcess(command_line, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to run command: "
               << command_line.GetCommandLineString();
    return false;
  }

  if (!process.WaitForExitWithTimeout(timeout, exit_code)) {
    process.Terminate(0, false);
    LOG(ERROR) << "Timeout exceeded for command: "
               << command_line.GetCommandLineString();
    return false;
  }

  return true;
}

bool RunHostScript(const std::vector<std::string>& args, int* exit_code) {
  return RunHostScriptWithTimeout(
      args, base::TimeDelta::FromMilliseconds(kDaemonTimeoutMs), exit_code);
}

}  // namespace

DaemonControllerDelegateLinux::DaemonControllerDelegateLinux() {
}

DaemonControllerDelegateLinux::~DaemonControllerDelegateLinux() {
}

DaemonController::State DaemonControllerDelegateLinux::GetState() {
  base::FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    LOG(ERROR) << "GetScriptPath() failed.";
    return DaemonController::STATE_UNKNOWN;
  }
  base::CommandLine command_line(script_path);
  command_line.AppendArg("--get-status");

  std::string status;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command_line, &status, &exit_code) ||
      exit_code != 0) {
    LOG(ERROR) << "Failed to run \"" << command_line.GetCommandLineString()
               << "\". Exit code: " << exit_code;
    return DaemonController::STATE_UNKNOWN;
  }

  base::TrimWhitespaceASCII(status, base::TRIM_ALL, &status);

  if (status == "STARTED") {
    return DaemonController::STATE_STARTED;
  } else if (status == "STOPPED") {
    return DaemonController::STATE_STOPPED;
  } else if (status == "NOT_IMPLEMENTED") {
    // Chrome Remote Desktop is not currently supported on the underlying Linux
    // Distro.
    return DaemonController::STATE_NOT_IMPLEMENTED;
  } else {
    LOG(ERROR) << "Unknown status string returned from  \""
               << command_line.GetCommandLineString()
               << "\": " << status;
    return DaemonController::STATE_UNKNOWN;
  }
}

scoped_ptr<base::DictionaryValue> DaemonControllerDelegateLinux::GetConfig() {
  scoped_ptr<base::DictionaryValue> config(
      HostConfigFromJsonFile(GetConfigPath()));
  if (!config)
    return nullptr;

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  std::string value;
  if (config->GetString(kHostIdConfigPath, &value)) {
    result->SetString(kHostIdConfigPath, value);
  }
  if (config->GetString(kXmppLoginConfigPath, &value)) {
    result->SetString(kXmppLoginConfigPath, value);
  }
  return result.Pass();
}

void DaemonControllerDelegateLinux::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const DaemonController::CompletionCallback& done) {
  // Add the user to chrome-remote-desktop group first.
  std::vector<std::string> args;
  args.push_back("--add-user");
  int exit_code;
  if (!RunHostScriptWithTimeout(
          args, base::TimeDelta::FromSeconds(kSudoTimeoutSeconds),
          &exit_code) ||
      exit_code != 0) {
    LOG(ERROR) << "Failed to add user to chrome-remote-desktop group.";
    done.Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Ensure the configuration directory exists.
  base::FilePath config_dir = GetConfigPath().DirName();
  if (!base::DirectoryExists(config_dir) &&
      !base::CreateDirectory(config_dir)) {
    LOG(ERROR) << "Failed to create config directory " << config_dir.value();
    done.Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Write config.
  if (!HostConfigToJsonFile(*config, GetConfigPath())) {
    LOG(ERROR) << "Failed to update config file.";
    done.Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Finally start the host.
  args.clear();
  args.push_back("--start");
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;
  if (RunHostScript(args, &exit_code) && (exit_code == 0))
    result = DaemonController::RESULT_OK;

  done.Run(result);
}

void DaemonControllerDelegateLinux::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const DaemonController::CompletionCallback& done) {
  scoped_ptr<base::DictionaryValue> new_config(
      HostConfigFromJsonFile(GetConfigPath()));
  if (new_config)
    new_config->MergeDictionary(config.get());
  if (!new_config || !HostConfigToJsonFile(*new_config, GetConfigPath())) {
    LOG(ERROR) << "Failed to update config file.";
    done.Run(DaemonController::RESULT_FAILED);
    return;
  }

  std::vector<std::string> args;
  args.push_back("--reload");
  int exit_code = 0;
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;
  if (RunHostScript(args, &exit_code) && (exit_code == 0))
    result = DaemonController::RESULT_OK;

  done.Run(result);
}

void DaemonControllerDelegateLinux::Stop(
    const DaemonController::CompletionCallback& done) {
  std::vector<std::string> args;
  args.push_back("--stop");
  int exit_code = 0;
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;
  if (RunHostScript(args, &exit_code) && (exit_code == 0))
    result = DaemonController::RESULT_OK;

  done.Run(result);
}

DaemonController::UsageStatsConsent
DaemonControllerDelegateLinux::GetUsageStatsConsent() {
  // Crash dump collection is not implemented on Linux yet.
  // http://crbug.com/130678.
  DaemonController::UsageStatsConsent consent;
  consent.supported = false;
  consent.allowed = false;
  consent.set_by_policy = false;
  return consent;
}

scoped_refptr<DaemonController> DaemonController::Create() {
  scoped_ptr<DaemonController::Delegate> delegate(
      new DaemonControllerDelegateLinux());
  return new DaemonController(delegate.Pass());
}

}  // namespace remoting
