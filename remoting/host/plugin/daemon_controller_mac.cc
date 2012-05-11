// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include <launch.h>
#include <sys/types.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launchd.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_launch_data.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "remoting/host/json_host_config.h"

namespace remoting {

namespace {

// The NSSystemDirectories.h header has a conflicting definition of
// NSSearchPathDirectory with the one in base/mac/foundation_util.h.
// Foundation.h would work, but it can only be included from Objective-C files.
// Therefore, we define the needed constants here.
const int NSLibraryDirectory = 5;

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

const int kPrefPaneWaitRetryLimit = 60;
const int kPrefPaneWaitTimeout = 1000;

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
  virtual void GetVersion(const GetVersionCallback& done_callback) OVERRIDE;

 private:
  void DoGetConfig(const GetConfigCallback& callback);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                           const CompletionCallback& done_callback);
  void DoUpdateConfig(scoped_ptr<base::DictionaryValue> config,
                      const CompletionCallback& done_callback);
  void DoStop(const CompletionCallback& done_callback);
  void NotifyOnState(DaemonController::State state,
                     const CompletionCallback& done_callback,
                     int tries_remaining,
                     const base::TimeDelta& sleep);
  bool ShowPreferencePane(const std::string& config_data);

  base::Thread auth_thread_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerMac);
};

DaemonControllerMac::DaemonControllerMac()
    : auth_thread_("Auth thread") {
  auth_thread_.Start();
}

DaemonControllerMac::~DaemonControllerMac() {
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

void DaemonControllerMac::GetVersion(const GetVersionCallback& done_callback) {
  NOTIMPLEMENTED();
  done_callback.Run("");
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
  std::string config_data;
  base::JSONWriter::Write(config.get(), &config_data);

  bool result = ShowPreferencePane(config_data);

  if (!result) {
    done_callback.Run(RESULT_FAILED);
  }

  // TODO(jamiewalch): Replace this with something a bit more robust
  NotifyOnState(DaemonController::STATE_STARTED,
                done_callback,
                kPrefPaneWaitRetryLimit,
                base::TimeDelta::FromMilliseconds(kPrefPaneWaitTimeout));
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

  std::string config_data = config_file.GetSerializedData();
  bool success = ShowPreferencePane(config_data);

  done_callback.Run(success ? RESULT_OK : RESULT_FAILED);
}

bool DaemonControllerMac::ShowPreferencePane(const std::string& config_data) {
  if (!config_data.empty()) {
    FilePath config_path;
    if (!file_util::GetTempDir(&config_path)) {
      LOG(ERROR) << "Failed to get filename for saving configuration data.";
      return false;
    }
    config_path = config_path.Append(kServiceName ".json");

    int written = file_util::WriteFile(config_path, config_data.data(),
                                       config_data.size());
    if (written != static_cast<int>(config_data.size())) {
      LOG(ERROR) << "Failed to save configuration data to: "
                 << config_path.value();
      return false;
    }
  }

  FilePath pane_path;
  // TODO(lambroslambrou): Use NSPreferencePanesDirectory once we start
  // building against SDK 10.6.
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &pane_path)) {
    LOG(ERROR) << "Failed to get directory for local preference panes.";
    return false;
  }
  pane_path = pane_path.Append("PreferencePanes")
      .Append(kServiceName ".prefPane");

  FSRef pane_path_ref;
  if (!base::mac::FSRefFromPath(pane_path.value(), &pane_path_ref)) {
    LOG(ERROR) << "Failed to create FSRef";
    return false;
  }
  OSStatus status = LSOpenFSRef(&pane_path_ref, NULL);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "LSOpenFSRef failed for path: "
                                << pane_path.value();
    return false;
  }

  CFNotificationCenterRef center =
      CFNotificationCenterGetDistributedCenter();
  CFNotificationCenterPostNotification(center, CFSTR(kServiceName), NULL, NULL,
                                       TRUE);
  return true;
}

void DaemonControllerMac::DoStop(const CompletionCallback& done_callback) {
  if (!ShowPreferencePane("")) {
    done_callback.Run(RESULT_FAILED);
    return;
  }

  // TODO(jamiewalch): Replace this with something a bit more robust
  NotifyOnState(DaemonController::STATE_STOPPED,
                done_callback,
                kPrefPaneWaitRetryLimit,
                base::TimeDelta::FromMilliseconds(kPrefPaneWaitTimeout));
}

void DaemonControllerMac::NotifyOnState(
    DaemonController::State state,
    const CompletionCallback& done_callback,
    int tries_remaining,
    const base::TimeDelta& sleep) {
  if (GetState() == state) {
    done_callback.Run(RESULT_OK);
  } else if (tries_remaining == 0) {
    done_callback.Run(RESULT_FAILED);
  } else {
    auth_thread_.message_loop_proxy()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DaemonControllerMac::NotifyOnState,
                   base::Unretained(this),
                   state,
                   done_callback,
                   tries_remaining - 1,
                   sleep),
        sleep);
  }
}

}  // namespace

scoped_ptr<DaemonController> remoting::DaemonController::Create() {
  return scoped_ptr<DaemonController>(new DaemonControllerMac());
}

}  // namespace remoting
