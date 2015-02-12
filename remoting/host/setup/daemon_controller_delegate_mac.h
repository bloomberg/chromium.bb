// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonControllerDelegateMac : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateMac();
  ~DaemonControllerDelegateMac() override;

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  scoped_ptr<base::DictionaryValue> GetConfig() override;
  void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      bool consent,
      const DaemonController::CompletionCallback& done) override;
  void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                    const DaemonController::CompletionCallback& done) override;
  void Stop(const DaemonController::CompletionCallback& done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;

 private:
  void ShowPreferencePane(const std::string& config_data,
                          const DaemonController::CompletionCallback& done);
  void RegisterForPreferencePaneNotifications(
      const DaemonController::CompletionCallback &done);
  void DeregisterForPreferencePaneNotifications();
  void PreferencePaneCallbackDelegate(CFStringRef name);

  static bool DoShowPreferencePane(const std::string& config_data);
  static void PreferencePaneCallback(CFNotificationCenterRef center,
                                     void* observer,
                                     CFStringRef name,
                                     const void* object,
                                     CFDictionaryRef user_info);

  DaemonController::CompletionCallback current_callback_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerDelegateMac);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_MAC_H_
