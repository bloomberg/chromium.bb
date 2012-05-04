// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include <launch.h>
#include <sys/types.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/mac/authorization_util.h"
#include "base/mac/launchd.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_authorizationref.h"
#include "base/mac/scoped_launch_data.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "remoting/host/json_host_config.h"

namespace remoting {

namespace {

// The name of the Remoting Host service that is registered with launchd.
#define kServiceName "org.chromium.chromoting"
#define kConfigDir "/Library/PrivilegedHelperTools/"

// This helper script is executed as root.  It is passed a command-line option
// (--enable or --disable), which causes it to create or remove a file that
// informs the host's launch script of whether the host is enabled or disabled.
const char kStartStopTool[] = kConfigDir kServiceName ".me2me.sh";

// Use a single configuration file, instead of separate "auth" and "host" files.
// This is because the SetConfigAndStart() API only provides a single
// dictionary, and splitting this into two dictionaries would require
// knowledge of which keys belong in which files.
const char kHostConfigFile[] = kConfigDir kServiceName ".json";

const int kStopWaitRetryLimit = 20;
const int kStopWaitTimeout = 500;

class DaemonControllerMac : public remoting::DaemonController {
 public:
  DaemonControllerMac();
  virtual ~DaemonControllerMac();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      const CompletionCallback& done_callback) OVERRIDE;
  virtual void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                            const CompletionCallback& done_callback) OVERRIDE;
  virtual void Stop(const CompletionCallback& done_callback) OVERRIDE;
  virtual void SetWindow(void* window_handle) OVERRIDE;

 private:
  void DoGetConfig(const GetConfigCallback& callback);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                           const CompletionCallback& done_callback);
  void DoUpdateConfig(scoped_ptr<base::DictionaryValue> config,
                      const CompletionCallback& done_callback);
  void DoStop(const CompletionCallback& done_callback);
  void NotifyWhenStopped(const CompletionCallback& done_callback,
                         int tries_remaining,
                         const base::TimeDelta& sleep);

  bool RunToolScriptAsRoot(const char* command, const std::string& input_data);
  bool SendJobControlMessage(const char* launchKey);

  // The API for gaining root privileges is blocking (it prompts the user for
  // a password). Since Start() and Stop() must not block the main thread, they
  // need to post their tasks to a separate thread.
  base::Thread auth_thread_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerMac);
};

DaemonControllerMac::DaemonControllerMac()
    : auth_thread_("Auth thread") {
  auth_thread_.Start();
}

DaemonControllerMac::~DaemonControllerMac() {
  // This will block if the thread is waiting on a root password prompt.  There
  // doesn't seem to be an easy solution for this, other than to spawn a
  // separate process to do the root elevation.

  // TODO(lambroslambrou): Improve this, either by finding a way to terminate
  // the thread, or by moving to a separate process.
  auth_thread_.Stop();
}

DaemonController::State DaemonControllerMac::GetState() {
  pid_t job_pid = base::mac::PIDForJob(kServiceName);
  if (job_pid < 0) {
    return DaemonController::STATE_NOT_INSTALLED;
  } else if (job_pid == 0) {
    // Service is stopped, or a start attempt failed.
    return DaemonController::STATE_STOPPED;
  } else {
    return DaemonController::STATE_STARTED;
  }
}

void DaemonControllerMac::GetConfig(const GetConfigCallback& callback) {
  // base::Unretained() is safe, since this object owns the thread and therefore
  // outlives it.
  auth_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerMac::DoGetConfig, base::Unretained(this),
                 callback));
}

void DaemonControllerMac::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  auth_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerMac::DoSetConfigAndStart, base::Unretained(this),
          base::Passed(&config), done_callback));
}

void DaemonControllerMac::UpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  auth_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DaemonControllerMac::DoUpdateConfig, base::Unretained(this),
      base::Passed(&config), done_callback));
}

void DaemonControllerMac::Stop(const CompletionCallback& done_callback) {
  auth_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerMac::DoStop, base::Unretained(this), done_callback));
}

void DaemonControllerMac::SetWindow(void* window_handle) {
  // noop
}

void DaemonControllerMac::DoGetConfig(const GetConfigCallback& callback) {
  FilePath config_path(kHostConfigFile);
  JsonHostConfig host_config(config_path);
  scoped_ptr<base::DictionaryValue> config;

  if (host_config.Read()) {
    config.reset(new base::DictionaryValue());
    std::string value;
    if (host_config.GetString(kHostIdConfigPath, &value))
      config.get()->SetString(kHostIdConfigPath, value);
    if (host_config.GetString(kXmppLoginConfigPath, &value))
      config.get()->SetString(kXmppLoginConfigPath, value);
  }

  callback.Run(config.Pass());
}

void DaemonControllerMac::DoSetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  std::string file_content;
  base::JSONWriter::Write(config.get(), &file_content);
  if (!RunToolScriptAsRoot("--enable", file_content)) {
    done_callback.Run(RESULT_FAILED);
    return;
  }
  bool result = SendJobControlMessage(LAUNCH_KEY_STARTJOB);
  done_callback.Run(result ? RESULT_OK : RESULT_FAILED);
}

void DaemonControllerMac::DoUpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  FilePath config_file_path(kHostConfigFile);
  JsonHostConfig config_file(config_file_path);
  if (!config_file.Read()) {
    done_callback.Run(RESULT_FAILED);
    return;
  }
  for (DictionaryValue::key_iterator key(config->begin_keys());
       key != config->end_keys(); ++key) {
    std::string value;
    if (!config->GetString(*key, &value)) {
      LOG(ERROR) << *key << " is not a string.";
      done_callback.Run(RESULT_FAILED);
      return;
    }
    config_file.SetString(*key, value);
  }

  std::string file_content = config_file.GetSerializedData();
  if (!RunToolScriptAsRoot("--save-config", file_content)) {
      done_callback.Run(RESULT_FAILED);
      return;
  }

  done_callback.Run(RESULT_OK);
  pid_t job_pid = base::mac::PIDForJob(kServiceName);
  if (job_pid > 0) {
    kill(job_pid, SIGHUP);
  }
}

void DaemonControllerMac::DoStop(const CompletionCallback& done_callback) {
  if (!RunToolScriptAsRoot("--disable", "")) {
    done_callback.Run(RESULT_FAILED);
    return;
  }

  // Deleting the trigger file does not cause launchd to stop the service.
  // Since the service is running for the local user's desktop (not as root),
  // it has to be stopped for that user.  This cannot easily be done in the
  // shell-script running as root, so it is done here instead.
  if (!SendJobControlMessage(LAUNCH_KEY_STOPJOB)) {
    done_callback.Run(RESULT_FAILED);
    return;
  }

  // SendJobControlMessage does not wait for the stop to take effect, so we
  // can't return immediately. Instead, we wait up to 10s.
  NotifyWhenStopped(done_callback,
                    kStopWaitRetryLimit,
                    base::TimeDelta::FromMilliseconds(kStopWaitTimeout));
}

void DaemonControllerMac::NotifyWhenStopped(
    const CompletionCallback& done_callback,
    int tries_remaining,
    const base::TimeDelta& sleep) {
  if (GetState() == DaemonController::STATE_STOPPED) {
    done_callback.Run(RESULT_OK);
  } else if (tries_remaining == 0) {
    done_callback.Run(RESULT_FAILED);
  } else {
    auth_thread_.message_loop_proxy()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DaemonControllerMac::NotifyWhenStopped,
                   base::Unretained(this),
                   done_callback,
                   tries_remaining - 1,
                   sleep),
        sleep);
  }
}

bool DaemonControllerMac::RunToolScriptAsRoot(const char* command,
                                              const std::string& input_data) {
  // TODO(lambroslambrou): Supply a localized prompt string here.
  base::mac::ScopedAuthorizationRef authorization(
      base::mac::AuthorizationCreateToRunAsRoot(CFSTR("")));
  if (!authorization) {
    LOG(ERROR) << "Failed to get root privileges.";
    return false;
  }

  if (!file_util::VerifyPathControlledByAdmin(FilePath(kStartStopTool))) {
    LOG(ERROR) << "Security check failed for: " << kStartStopTool;
    return false;
  }

  // TODO(lambroslambrou): Use sandbox-exec to minimize exposure -
  // http://crbug.com/120903
  const char* arguments[] = { command, NULL };
  FILE* pipe = NULL;
  pid_t pid;
  OSStatus status = base::mac::ExecuteWithPrivilegesAndGetPID(
      authorization.get(),
      kStartStopTool,
      kAuthorizationFlagDefaults,
      arguments,
      &pipe,
      &pid);
  if (status != errAuthorizationSuccess) {
    OSSTATUS_LOG(ERROR, status) << "AuthorizationExecuteWithPrivileges";
    return false;
  }
  if (pid == -1) {
    LOG(ERROR) << "Failed to get child PID";
    return false;
  }

  DCHECK(pipe);
  if (!input_data.empty()) {
    size_t bytes_written = fwrite(input_data.data(), sizeof(char),
                                  input_data.size(), pipe);
    // According to the fwrite manpage, a partial count is returned only if a
    // write error has occurred.
    if (bytes_written != input_data.size()) {
      LOG(ERROR) << "Failed to write data to child process";
    }
    // Need to close, since the child waits for EOF on its stdin.
    if (fclose(pipe) != 0) {
      PLOG(ERROR) << "fclose";
    }
  }

  int exit_status;
  pid_t wait_result = HANDLE_EINTR(waitpid(pid, &exit_status, 0));
  if (wait_result != pid) {
    PLOG(ERROR) << "waitpid";
    return false;
  }
  if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0) {
    return true;
  } else {
    LOG(ERROR) << kStartStopTool << " failed with exit status " << exit_status;
    return false;
  }
}

bool DaemonControllerMac::SendJobControlMessage(const char* launchKey) {
  base::mac::ScopedLaunchData response(
      base::mac::MessageForJob(kServiceName, launchKey));
  if (!response) {
    LOG(ERROR) << "Failed to send message to launchd";
    return false;
  }

  // Got a response, so check if launchd sent a non-zero error code, otherwise
  // assume the command was successful.
  if (launch_data_get_type(response.get()) == LAUNCH_DATA_ERRNO) {
    int error = launch_data_get_errno(response.get());
    if (error) {
      LOG(ERROR) << "launchd returned error " << error;
      return false;
    }
  }
  return true;
}

}  // namespace

scoped_ptr<DaemonController> remoting::DaemonController::Create() {
  return scoped_ptr<DaemonController>(new DaemonControllerMac());
}

}  // namespace remoting
