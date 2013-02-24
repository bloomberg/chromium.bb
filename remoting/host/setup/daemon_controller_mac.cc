// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <launch.h>
#include <stdio.h>
#include <sys/types.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
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
#include "remoting/host/constants_mac.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/usage_stats_consent.h"

namespace remoting {

namespace {

// The NSSystemDirectories.h header has a conflicting definition of
// NSSearchPathDirectory with the one in base/mac/foundation_util.h.
// Foundation.h would work, but it can only be included from Objective-C files.
// Therefore, we define the needed constants here.
const int NSLibraryDirectory = 5;

class DaemonControllerMac : public remoting::DaemonController {
 public:
  DaemonControllerMac();
  virtual ~DaemonControllerMac();

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
      const GetUsageStatsConsentCallback& callback) OVERRIDE;

 private:
  void DoGetConfig(const GetConfigCallback& callback);
  void DoGetVersion(const GetVersionCallback& callback);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                           bool consent,
                           const CompletionCallback& done);
  void DoUpdateConfig(scoped_ptr<base::DictionaryValue> config,
                      const CompletionCallback& done_callback);
  void DoStop(const CompletionCallback& done_callback);
  void DoGetUsageStatsConsent(const GetUsageStatsConsentCallback& callback);

  void ShowPreferencePane(const std::string& config_data,
                          const CompletionCallback& done_callback);
  void RegisterForPreferencePaneNotifications(
      const CompletionCallback &done_callback);
  void DeregisterForPreferencePaneNotifications();
  void PreferencePaneCallbackDelegate(CFStringRef name);
  static bool DoShowPreferencePane(const std::string& config_data);
  static void PreferencePaneCallback(CFNotificationCenterRef center,
                                     void* observer,
                                     CFStringRef name,
                                     const void* object,
                                     CFDictionaryRef user_info);

  base::Thread auth_thread_;
  CompletionCallback current_callback_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerMac);
};

DaemonControllerMac::DaemonControllerMac()
    : auth_thread_("Auth thread") {
  auth_thread_.Start();
}

DaemonControllerMac::~DaemonControllerMac() {
  auth_thread_.Stop();
  DeregisterForPreferencePaneNotifications();
}

void DaemonControllerMac::DeregisterForPreferencePaneNotifications() {
  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      CFSTR(UPDATE_SUCCEEDED_NOTIFICATION_NAME),
      NULL);
  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      CFSTR(UPDATE_FAILED_NOTIFICATION_NAME),
      NULL);
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
    bool consent,
    const CompletionCallback& done) {
  auth_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &DaemonControllerMac::DoSetConfigAndStart, base::Unretained(this),
          base::Passed(&config), consent, done));
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

void DaemonControllerMac::GetVersion(const GetVersionCallback& callback) {
  auth_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerMac::DoGetVersion, base::Unretained(this),
                 callback));
}

void DaemonControllerMac::GetUsageStatsConsent(
    const GetUsageStatsConsentCallback& callback) {
  auth_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DaemonControllerMac::DoGetUsageStatsConsent,
                 base::Unretained(this), callback));
}

void DaemonControllerMac::DoGetConfig(const GetConfigCallback& callback) {
  base::FilePath config_path(kHostConfigFilePath);
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

void DaemonControllerMac::DoGetVersion(const GetVersionCallback& callback) {
  std::string version = "";
  std::string command_line = remoting::kHostHelperScriptPath;
  command_line += " --host-version";
  FILE* script_output = popen(command_line.c_str(), "r");
  if (script_output) {
    char buffer[100];
    char* result = fgets(buffer, sizeof(buffer), script_output);
    pclose(script_output);
    if (result) {
      // The string is guaranteed to be null-terminated, but probably contains
      // a newline character, which we don't want.
      for (int i = 0; result[i]; ++i) {
        if (result[i] < ' ') {
          result[i] = 0;
          break;
        }
      }
      version = result;
    }
  }
  callback.Run(version);
}

void DaemonControllerMac::DoSetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const CompletionCallback& done) {
  config->SetBoolean(kUsageStatsConsentConfigPath, consent);
  std::string config_data;
  base::JSONWriter::Write(config.get(), &config_data);
  ShowPreferencePane(config_data, done);
}

void DaemonControllerMac::DoUpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done_callback) {
  base::FilePath config_file_path(kHostConfigFilePath);
  JsonHostConfig config_file(config_file_path);
  if (!config_file.Read()) {
    done_callback.Run(RESULT_FAILED);
    return;
  }
  if (!config_file.CopyFrom(config.get())) {
    LOG(ERROR) << "Failed to update configuration.";
    done_callback.Run(RESULT_FAILED);
    return;
  }

  std::string config_data = config_file.GetSerializedData();
  ShowPreferencePane(config_data, done_callback);
}

void DaemonControllerMac::DoGetUsageStatsConsent(
    const GetUsageStatsConsentCallback& callback) {
  bool allowed = false;
  base::FilePath config_file_path(kHostConfigFilePath);
  JsonHostConfig host_config(config_file_path);
  if (host_config.Read()) {
    host_config.GetBoolean(kUsageStatsConsentConfigPath, &allowed);
  }
  // set_by_policy is not yet supported.
  callback.Run(true, allowed, false /* set_by_policy */);
}

void DaemonControllerMac::ShowPreferencePane(
    const std::string& config_data, const CompletionCallback& done_callback) {
  if (DoShowPreferencePane(config_data)) {
    RegisterForPreferencePaneNotifications(done_callback);
  } else {
    done_callback.Run(RESULT_FAILED);
  }
}

bool DaemonControllerMac::DoShowPreferencePane(const std::string& config_data) {
  if (!config_data.empty()) {
    base::FilePath config_path;
    if (!file_util::GetTempDir(&config_path)) {
      LOG(ERROR) << "Failed to get filename for saving configuration data.";
      return false;
    }
    config_path = config_path.Append(kHostConfigFileName);

    int written = file_util::WriteFile(config_path, config_data.data(),
                                       config_data.size());
    if (written != static_cast<int>(config_data.size())) {
      LOG(ERROR) << "Failed to save configuration data to: "
                 << config_path.value();
      return false;
    }
  }

  base::FilePath pane_path;
  // TODO(lambroslambrou): Use NSPreferencePanesDirectory once we start
  // building against SDK 10.6.
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &pane_path)) {
    LOG(ERROR) << "Failed to get directory for local preference panes.";
    return false;
  }
  pane_path = pane_path.Append("PreferencePanes").Append(kPrefPaneFileName);

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
  base::mac::ScopedCFTypeRef<CFStringRef> service_name(
        CFStringCreateWithCString(kCFAllocatorDefault, remoting::kServiceName,
                                  kCFStringEncodingUTF8));
  CFNotificationCenterPostNotification(center, service_name, NULL, NULL,
                                       TRUE);
  return true;
}

void DaemonControllerMac::DoStop(const CompletionCallback& done_callback) {
  ShowPreferencePane("", done_callback);
}

// CFNotificationCenterAddObserver ties the thread on which distributed
// notifications are received to the one on which it is first called.
// This is safe because HostNPScriptObject::InvokeAsyncResultCallback
// bounces the invocation to the correct thread, so it doesn't matter
// which thread CompletionCallbacks are called on.
void DaemonControllerMac::RegisterForPreferencePaneNotifications(
    const CompletionCallback& done_callback) {
  // We can only have one callback registered at a time. This is enforced by the
  // UX flow of the web-app.
  DCHECK(current_callback_.is_null());
  current_callback_ = done_callback;

  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      &DaemonControllerMac::PreferencePaneCallback,
      CFSTR(UPDATE_SUCCEEDED_NOTIFICATION_NAME),
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      &DaemonControllerMac::PreferencePaneCallback,
      CFSTR(UPDATE_FAILED_NOTIFICATION_NAME),
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

void DaemonControllerMac::PreferencePaneCallbackDelegate(CFStringRef name) {
  AsyncResult result = RESULT_FAILED;
  if (CFStringCompare(name, CFSTR(UPDATE_SUCCEEDED_NOTIFICATION_NAME), 0) ==
          kCFCompareEqualTo) {
    result = RESULT_OK;
  } else if (CFStringCompare(name, CFSTR(UPDATE_FAILED_NOTIFICATION_NAME), 0) ==
          kCFCompareEqualTo) {
    result = RESULT_FAILED;
  } else {
    LOG(WARNING) << "Ignoring unexpected notification: " << name;
    return;
  }
  DCHECK(!current_callback_.is_null());
  current_callback_.Run(result);
  current_callback_.Reset();
  DeregisterForPreferencePaneNotifications();
}

void DaemonControllerMac::PreferencePaneCallback(CFNotificationCenterRef center,
                                                 void* observer,
                                                 CFStringRef name,
                                                 const void* object,
                                                 CFDictionaryRef user_info) {
  DaemonControllerMac* self = reinterpret_cast<DaemonControllerMac*>(observer);
  if (self) {
    self->PreferencePaneCallbackDelegate(name);
  } else {
    LOG(WARNING) << "Ignoring notification with NULL observer: " << name;
  }
}

}  // namespace

scoped_ptr<DaemonController> remoting::DaemonController::Create() {
  return scoped_ptr<DaemonController>(new DaemonControllerMac());
}

}  // namespace remoting
