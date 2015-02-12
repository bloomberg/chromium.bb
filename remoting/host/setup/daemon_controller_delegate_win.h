// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_

#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

class DaemonInstallerWin;

class DaemonControllerDelegateWin : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateWin();
  virtual ~DaemonControllerDelegateWin();

  // DaemonController::Delegate interface.
  virtual DaemonController::State GetState() override;
  virtual scoped_ptr<base::DictionaryValue> GetConfig() override;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config,
      bool consent,
      const DaemonController::CompletionCallback& done) override;
  virtual void UpdateConfig(
      scoped_ptr<base::DictionaryValue> config,
      const DaemonController::CompletionCallback& done) override;
  virtual void Stop(const DaemonController::CompletionCallback& done) override;
  virtual DaemonController::UsageStatsConsent GetUsageStatsConsent() override;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerDelegateWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
