// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_mac.h"

#import <AppKit/AppKit.h>

#include <CoreFoundation/CoreFoundation.h>
#include <launch.h>
#include <stdio.h>
#include <sys/types.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launchd.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_launch_data.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "remoting/host/host_config.h"
#include "remoting/host/mac/constants_mac.h"
#include "remoting/host/usage_stats_consent.h"

namespace remoting {

DaemonControllerDelegateMac::DaemonControllerDelegateMac() {
}

DaemonControllerDelegateMac::~DaemonControllerDelegateMac() {
  DeregisterForPreferencePaneNotifications();
}

DaemonController::State DaemonControllerDelegateMac::GetState() {
  pid_t job_pid = base::mac::PIDForJob(kServiceName);
  if (job_pid < 0) {
    return DaemonController::STATE_UNKNOWN;
  } else if (job_pid == 0) {
    // Service is stopped, or a start attempt failed.
    return DaemonController::STATE_STOPPED;
  } else {
    return DaemonController::STATE_STARTED;
  }
}

std::unique_ptr<base::DictionaryValue>
DaemonControllerDelegateMac::GetConfig() {
  base::FilePath config_path(kHostConfigFilePath);
  std::unique_ptr<base::DictionaryValue> host_config(
      HostConfigFromJsonFile(config_path));
  if (!host_config)
    return nullptr;

  std::unique_ptr<base::DictionaryValue> config(new base::DictionaryValue);
  std::string value;
  if (host_config->GetString(kHostIdConfigPath, &value))
    config->SetString(kHostIdConfigPath, value);
  if (host_config->GetString(kXmppLoginConfigPath, &value))
    config->SetString(kXmppLoginConfigPath, value);
  return config;
}

void DaemonControllerDelegateMac::SetConfigAndStart(
    std::unique_ptr<base::DictionaryValue> config,
    bool consent,
    const DaemonController::CompletionCallback& done) {
  config->SetBoolean(kUsageStatsConsentConfigPath, consent);
  ShowPreferencePane(HostConfigToJson(*config), done);
}

void DaemonControllerDelegateMac::UpdateConfig(
    std::unique_ptr<base::DictionaryValue> config,
    const DaemonController::CompletionCallback& done) {
  base::FilePath config_file_path(kHostConfigFilePath);
  std::unique_ptr<base::DictionaryValue> host_config(
      HostConfigFromJsonFile(config_file_path));
  if (!host_config) {
    done.Run(DaemonController::RESULT_FAILED);
    return;
  }

  host_config->MergeDictionary(config.get());
  ShowPreferencePane(HostConfigToJson(*host_config), done);
}

void DaemonControllerDelegateMac::Stop(
    const DaemonController::CompletionCallback& done) {
  ShowPreferencePane("", done);
}

DaemonController::UsageStatsConsent
DaemonControllerDelegateMac::GetUsageStatsConsent() {
  DaemonController::UsageStatsConsent consent;
  consent.supported = true;
  consent.allowed = false;
  // set_by_policy is not yet supported.
  consent.set_by_policy = false;

  base::FilePath config_file_path(kHostConfigFilePath);
  std::unique_ptr<base::DictionaryValue> host_config(
      HostConfigFromJsonFile(config_file_path));
  if (host_config) {
    host_config->GetBoolean(kUsageStatsConsentConfigPath, &consent.allowed);
  }

  return consent;
}

void DaemonControllerDelegateMac::ShowPreferencePane(
    const std::string& config_data,
    const DaemonController::CompletionCallback& done) {
  if (DoShowPreferencePane(config_data)) {
    RegisterForPreferencePaneNotifications(done);
  } else {
    done.Run(DaemonController::RESULT_FAILED);
  }
}

// CFNotificationCenterAddObserver ties the thread on which distributed
// notifications are received to the one on which it is first called.
// This is safe because HostNPScriptObject::InvokeAsyncResultCallback
// bounces the invocation to the correct thread, so it doesn't matter
// which thread CompletionCallbacks are called on.
void DaemonControllerDelegateMac::RegisterForPreferencePaneNotifications(
    const DaemonController::CompletionCallback& done) {
  // We can only have one callback registered at a time. This is enforced by the
  // UX flow of the web-app.
  DCHECK(current_callback_.is_null());
  current_callback_ = done;

  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      &DaemonControllerDelegateMac::PreferencePaneCallback,
      CFSTR(UPDATE_SUCCEEDED_NOTIFICATION_NAME),
      nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      &DaemonControllerDelegateMac::PreferencePaneCallback,
      CFSTR(UPDATE_FAILED_NOTIFICATION_NAME),
      nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

void DaemonControllerDelegateMac::DeregisterForPreferencePaneNotifications() {
  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      CFSTR(UPDATE_SUCCEEDED_NOTIFICATION_NAME),
      nullptr);
  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(),
      this,
      CFSTR(UPDATE_FAILED_NOTIFICATION_NAME),
      nullptr);
}

void DaemonControllerDelegateMac::PreferencePaneCallbackDelegate(
    CFStringRef name) {
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;
  if (CFStringCompare(name, CFSTR(UPDATE_SUCCEEDED_NOTIFICATION_NAME), 0) ==
          kCFCompareEqualTo) {
    result = DaemonController::RESULT_OK;
  } else if (CFStringCompare(name, CFSTR(UPDATE_FAILED_NOTIFICATION_NAME), 0) ==
          kCFCompareEqualTo) {
    result = DaemonController::RESULT_FAILED;
  } else {
    LOG(WARNING) << "Ignoring unexpected notification: " << name;
    return;
  }

  DeregisterForPreferencePaneNotifications();

  DCHECK(!current_callback_.is_null());
  base::ResetAndReturn(&current_callback_).Run(result);
}

// static
bool DaemonControllerDelegateMac::DoShowPreferencePane(
    const std::string& config_data) {
  if (!config_data.empty()) {
    base::FilePath config_path;
    if (!base::GetTempDir(&config_path)) {
      LOG(ERROR) << "Failed to get filename for saving configuration data.";
      return false;
    }
    config_path = config_path.Append(kHostConfigFileName);

    int written = base::WriteFile(config_path, config_data.data(),
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

  bool success = [[NSWorkspace sharedWorkspace]
      openFile:base::SysUTF8ToNSString(pane_path.value())];
  if (!success) {
    LOG(ERROR) << "Failed to open preferences pane: " << pane_path.value();
    return false;
  }

  CFNotificationCenterRef center =
      CFNotificationCenterGetDistributedCenter();
  base::ScopedCFTypeRef<CFStringRef> service_name(CFStringCreateWithCString(
      kCFAllocatorDefault, remoting::kServiceName, kCFStringEncodingUTF8));
  CFNotificationCenterPostNotification(center, service_name, nullptr, nullptr,
                                       TRUE);
  return true;
}

// static
void DaemonControllerDelegateMac::PreferencePaneCallback(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef user_info) {
  DaemonControllerDelegateMac* self =
      reinterpret_cast<DaemonControllerDelegateMac*>(observer);
  if (!self) {
    LOG(WARNING) << "Ignoring notification with nullptr observer: " << name;
    return;
  }

  self->PreferencePaneCallbackDelegate(name);
}

scoped_refptr<DaemonController> DaemonController::Create() {
  return new DaemonController(
      base::WrapUnique(new DaemonControllerDelegateMac()));
}

}  // namespace remoting
