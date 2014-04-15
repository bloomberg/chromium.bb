// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonControllerDelegateLinux : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateLinux();
  virtual ~DaemonControllerDelegateLinux();

  // DaemonController::Delegate interface.
  virtual DaemonController::State GetState() OVERRIDE;
  virtual scoped_ptr<base::DictionaryValue> GetConfig() OVERRIDE;
  virtual void InstallHost(
      const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      bool consent,
      const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void UpdateConfig(
      scoped_ptr<base::DictionaryValue> config,
      const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void Stop(const DaemonController::CompletionCallback& done) OVERRIDE;
  virtual void SetWindow(void* window_handle) OVERRIDE;
  virtual std::string GetVersion() OVERRIDE;
  virtual DaemonController::UsageStatsConsent GetUsageStatsConsent() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonControllerDelegateLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_LINUX_H_
