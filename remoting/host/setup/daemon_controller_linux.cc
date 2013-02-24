// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <unistd.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "remoting/host/host_config.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/usage_stats_consent.h"

namespace remoting {

namespace {

const char kDaemonScript[] =
    "/opt/google/chrome-remote-desktop/chrome-remote-desktop";

// Timeout for running daemon script.
const int64 kDaemonTimeoutMs = 5000;

// Timeout for commands that require password prompt - 5 minutes.
const int64 kSudoTimeoutSeconds = 5 * 60;

std::string GetMd5(const std::string& value) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, value);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return StringToLowerASCII(base::HexEncode(digest.a, sizeof(digest.a)));
}

class DaemonControllerLinux : public remoting::DaemonController {
 public:
  DaemonControllerLinux();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      bool consent,
      const CompletionCallback& done) OVERRIDE;
  virtual void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                            const CompletionCallback& done_callback) OVERRIDE;
  virtual void Stop(const CompletionCallback& done_callback) OVERRIDE;
  virtual void SetWindow(void* window_handle) OVERRIDE;
  virtual void GetVersion(const GetVersionCallback& done_callback) OVERRIDE;
  virtual void GetUsageStatsConsent(
      const GetUsageStatsConsentCallback& done) OVERRIDE;

 private:
  base::FilePath GetConfigPath();

  void DoGetConfig(const GetConfigCallback& callback);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                           const CompletionCallback& done);
  void DoUpdateConfig(scoped_ptr<base::DictionaryValue> config,
                      const CompletionCallback& done_callback);
  void DoStop(const CompletionCallback& done_callback);
  void DoGetVersion(const GetVersionCallback& done_callback);

  base::Thread file_io_thread_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerLinux);
};

DaemonControllerLinux::DaemonControllerLinux()
    : file_io_thread_("DaemonControllerFileIO") {
  file_io_thread_.Start();
}

static bool GetScriptPath(base::FilePath* result) {
  base::FilePath candidate_exe(kDaemonScript);
  if (access(candidate_exe.value().c_str(), X_OK) == 0) {
    *result = candidate_exe;
    return true;
  }
  return false;
}

static bool RunHostScriptWithTimeout(
    const std::vector<std::string>& args,
    base::TimeDelta timeout,
    int* exit_code) {
  DCHECK(exit_code);

  // As long as we're relying on running an external binary from the
  // PATH, don't do it as root.
  if (getuid() == 0) {
    return false;
  }
  base::FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    return false;
  }
  CommandLine command_line(script_path);
  for (unsigned int i = 0; i < args.size(); ++i) {
    command_line.AppendArg(args[i]);
  }
  base::ProcessHandle process_handle;
  if (!base::LaunchProcess(command_line, base::LaunchOptions(),
                           &process_handle)) {
    return false;
  }

  if (!base::WaitForExitCodeWithTimeout(process_handle, exit_code, timeout)) {
    base::KillProcess(process_handle, 0, false);
    return false;
  }

  return true;
}

static bool RunHostScript(const std::vector<std::string>& args,
                          int* exit_code) {
  return RunHostScriptWithTimeout(
      args, base::TimeDelta::FromMilliseconds(kDaemonTimeoutMs), exit_code);
}

remoting::DaemonController::State DaemonControllerLinux::GetState() {
  std::vector<std::string> args;
  args.push_back("--check-running");
  int exit_code = 0;
  if (!RunHostScript(args, &exit_code)) {
    // TODO(jamiewalch): When we have a good story for installing, return
    // NOT_INSTALLED rather than NOT_IMPLEMENTED (the former suppresses
    // the relevant UI in the web-app).
    return remoting::DaemonController::STATE_NOT_IMPLEMENTED;
  }

  if (exit_code == 0) {
    return remoting::DaemonController::STATE_STARTED;
  } else {
    return remoting::DaemonController::STATE_STOPPED;
  }
}

void DaemonControllerLinux::GetConfig(const GetConfigCallback& callback) {
  // base::Unretained() is safe because we control lifetime of the thread.
  file_io_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DaemonControllerLinux::DoGetConfig, base::Unretained(this), callback));
}

void DaemonControllerLinux::GetUsageStatsConsent(
    const GetUsageStatsConsentCallback& done) {
  // Crash dump collection is not implemented on Linux yet.
  // http://crbug.com/130678.
  done.Run(false, false, false);
}

void DaemonControllerLinux::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool /* consent */,
    const CompletionCallback& done) {
  // base::Unretained() is safe because we control lifetime of the thread.
  file_io_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DaemonControllerLinux::DoSetConfigAndStart, base::Unretained(this),
      base::Passed(&config), done));
}

void DaemonControllerLinux::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  file_io_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DaemonControllerLinux::DoUpdateConfig, base::Unretained(this),
      base::Passed(&config), done_callback));
}

void DaemonControllerLinux::Stop(const CompletionCallback& done_callback) {
  file_io_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DaemonControllerLinux::DoStop, base::Unretained(this),
      done_callback));
}

void DaemonControllerLinux::SetWindow(void* window_handle) {
  // noop
}

void DaemonControllerLinux::GetVersion(
    const GetVersionCallback& done_callback) {
  file_io_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DaemonControllerLinux::DoGetVersion, base::Unretained(this),
      done_callback));
}

base::FilePath DaemonControllerLinux::GetConfigPath() {
  std::string filename = "host#" + GetMd5(net::GetHostName()) + ".json";
  return file_util::GetHomeDir().
      Append(".config/chrome-remote-desktop").Append(filename);
}

void DaemonControllerLinux::DoGetConfig(const GetConfigCallback& callback) {
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  if (GetState() != remoting::DaemonController::STATE_NOT_IMPLEMENTED) {
    JsonHostConfig config(GetConfigPath());
    if (config.Read()) {
      std::string value;
      if (config.GetString(kHostIdConfigPath, &value)) {
        result->SetString(kHostIdConfigPath, value);
      }
      if (config.GetString(kXmppLoginConfigPath, &value)) {
        result->SetString(kXmppLoginConfigPath, value);
      }
    } else {
      result.reset(); // Return NULL in case of error.
    }
  }

  callback.Run(result.Pass());
}

void DaemonControllerLinux::DoSetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {

  // Add the user to chrome-remote-desktop group first.
  std::vector<std::string> args;
  args.push_back("--add-user");
  int exit_code;
  if (!RunHostScriptWithTimeout(
          args, base::TimeDelta::FromSeconds(kSudoTimeoutSeconds),
          &exit_code) ||
      exit_code != 0) {
    LOG(ERROR) << "Failed to add user to chrome-remote-desktop group.";
    done_callback.Run(RESULT_FAILED);
    return;
  }

  // Ensure the configuration directory exists.
  base::FilePath config_dir = GetConfigPath().DirName();
  if (!file_util::DirectoryExists(config_dir) &&
      !file_util::CreateDirectory(config_dir)) {
    LOG(ERROR) << "Failed to create config directory " << config_dir.value();
    done_callback.Run(RESULT_FAILED);
    return;
  }

  // Write config.
  JsonHostConfig config_file(GetConfigPath());
  if (!config_file.CopyFrom(config.get()) ||
      !config_file.Save()) {
    LOG(ERROR) << "Failed to update config file.";
    done_callback.Run(RESULT_FAILED);
    return;
  }

  // Finally start the host.
  args.clear();
  args.push_back("--start");
  AsyncResult result;
  if (RunHostScript(args, &exit_code)) {
    result = (exit_code == 0) ? RESULT_OK : RESULT_FAILED;
  } else {
    result = RESULT_FAILED;
  }
  done_callback.Run(result);
}

void DaemonControllerLinux::DoUpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  JsonHostConfig config_file(GetConfigPath());
  if (!config_file.Read() ||
      !config_file.CopyFrom(config.get()) ||
      !config_file.Save()) {
    LOG(ERROR) << "Failed to update config file.";
    done_callback.Run(RESULT_FAILED);
    return;
  }

  std::vector<std::string> args;
  args.push_back("--reload");
  AsyncResult result;
  int exit_code;
  if (RunHostScript(args, &exit_code)) {
    result = (exit_code == 0) ? RESULT_OK : RESULT_FAILED;
  } else {
    result = RESULT_FAILED;
  }

  done_callback.Run(result);
}

void DaemonControllerLinux::DoStop(const CompletionCallback& done_callback) {
  std::vector<std::string> args;
  args.push_back("--stop");
  int exit_code = 0;
  AsyncResult result;
  if (RunHostScript(args, &exit_code)) {
    result = (exit_code == 0) ? RESULT_OK : RESULT_FAILED;
  } else {
    result = RESULT_FAILED;
  }
  done_callback.Run(result);
}

void DaemonControllerLinux::DoGetVersion(
    const GetVersionCallback& done_callback) {
  base::FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    done_callback.Run("");
    return;
  }
  CommandLine command_line(script_path);
  command_line.AppendArg("--host-version");

  std::string version;
  int exit_code = 0;
  int result =
      base::GetAppOutputWithExitCode(command_line, &version, &exit_code);
  if (!result || exit_code != 0) {
    LOG(ERROR) << "Failed to run \"" << command_line.GetCommandLineString()
               << "\". Exit code: " << exit_code;
    done_callback.Run("");
    return;
  }

  TrimWhitespaceASCII(version, TRIM_ALL, &version);
  if (!ContainsOnlyChars(version, "0123456789.")) {
    LOG(ERROR) << "Received invalid host version number: " << version;
    done_callback.Run("");
    return;
  }

  done_callback.Run(version);
}

}  // namespace

scoped_ptr<DaemonController> remoting::DaemonController::Create() {
  return scoped_ptr<DaemonController>(new DaemonControllerLinux());
}

}  // namespace remoting
