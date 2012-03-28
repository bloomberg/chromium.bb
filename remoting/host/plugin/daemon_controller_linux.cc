// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include <unistd.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_split.h"

namespace remoting {

namespace {

const char* kDaemonScript = "me2me_virtual_host.py";
const int64 kDaemonTimeoutMs = 5000;

class DaemonControllerLinux : public remoting::DaemonController {
 public:
  DaemonControllerLinux();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config) OVERRIDE;
  virtual void SetPin(const std::string& pin) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonControllerLinux);
};

DaemonControllerLinux::DaemonControllerLinux() {
}

// TODO(jamiewalch): We'll probably be able to do a better job of
// detecting whether or not the daemon is installed once we have a
// proper installer. For now, detecting whether or not the binary
// is on the PATH is good enough.
static bool GetScriptPath(FilePath* result) {
  base::Environment* environment = base::Environment::Create();
  std::string path;
  if (environment->GetVar("PATH", &path)) {
    std::vector<std::string> path_directories;
    base::SplitString(path, ':', &path_directories);
    for (unsigned int i = 0; i < path_directories.size(); ++i) {
      FilePath candidate_exe(path_directories[i]);
      candidate_exe = candidate_exe.Append(kDaemonScript);
      if (access(candidate_exe.value().c_str(), X_OK) == 0) {
        *result = candidate_exe;
        return true;
      }
    }
  }
  return false;
}

static bool RunScript(const std::vector<std::string>& args, int* exit_code) {
  // As long as we're relying on running an external binary from the
  // PATH, don't do it as root.
  if (getuid() == 0) {
    return false;
  }
  FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    return false;
  }
  CommandLine command_line(script_path);
  for (unsigned int i = 0; i < args.size(); ++i) {
    command_line.AppendArg(args[i]);
  }
  base::ProcessHandle process_handle;
  bool result = base::LaunchProcess(command_line,
                                    base::LaunchOptions(),
                                    &process_handle);
  if (result) {
    if (exit_code) {
      result = base::WaitForExitCodeWithTimeout(process_handle,
                                                exit_code,
                                                kDaemonTimeoutMs);
    }
    base::CloseProcessHandle(process_handle);
  }
  return result;
}

remoting::DaemonController::State DaemonControllerLinux::GetState() {
  std::vector<std::string> args;
  args.push_back("--check-running");
  int exit_code = 0;
  if (!RunScript(args, &exit_code)) {
    // TODO(jamiewalch): When we have a good story for installing, return
    // NOT_INSTALLED rather than NOT_IMPLEMENTED (the former suppresses
    // the relevant UI in the web-app).
    return remoting::DaemonController::STATE_NOT_IMPLEMENTED;
  } else if (exit_code == 0) {
    return remoting::DaemonController::STATE_STARTED;
  } else {
    return remoting::DaemonController::STATE_STOPPED;
  }
}

void DaemonControllerLinux::GetConfig(const GetConfigCallback& callback) {
  NOTIMPLEMENTED();
}

void DaemonControllerLinux::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config) {
  // TODO(sergeyu): Save the |config|.
  // TODO(sergeyu): Set state to START_FAILED if RunScript() fails.
  std::vector<std::string> no_args;
  RunScript(no_args, NULL);
}

void DaemonControllerLinux::SetPin(const std::string& pin) {
  std::vector<std::string> args;
  args.push_back("--explicit-pin");
  args.push_back(pin);
  int exit_code = 0;
  RunScript(args, &exit_code);
}

// TODO(jamiewalch): If Stop is called after Start but before GetState returns
// STARTED, then it should prevent the daemon from starting; currently it will
// just fail silently.
void DaemonControllerLinux::Stop() {
  std::vector<std::string> args;
  args.push_back("--stop");
  // TODO(jamiewalch): Make Stop asynchronous like start once there's UI in the
  // web-app to support it.
  int exit_code = 0;
  RunScript(args, &exit_code);
}

}  // namespace

DaemonController* remoting::DaemonController::Create() {
  return new DaemonControllerLinux();
}

}  // namespace remoting
