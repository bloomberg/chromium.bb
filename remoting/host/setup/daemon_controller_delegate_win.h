// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "base/win/scoped_comptr.h"
// chromoting_lib.h contains MIDL-generated declarations.
#include "remoting/host/chromoting_lib.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/daemon_installer_win.h"

namespace remoting {

class DaemonInstallerWin;

class DaemonControllerDelegateWin : public DaemonController::Delegate {
 public:
  DaemonControllerDelegateWin();
  virtual ~DaemonControllerDelegateWin();

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
  // Activates an unprivileged instance of the daemon controller and caches it.
  HRESULT ActivateController();

  // Activates an instance of the daemon controller and caches it. If COM
  // Elevation is supported (Vista+) the activated instance is elevated,
  // otherwise it is activated under credentials of the caller.
  HRESULT ActivateElevatedController();

  // Releases the cached instance of the controller.
  void ReleaseController();

  // Install the host and then invoke the callback.
  void DoInstallHost(const DaemonInstallerWin::CompletionCallback& done);

  // Procedes with the daemon configuration if the installation succeeded,
  // otherwise reports the error.
  void StartHostWithConfig(
      scoped_ptr<base::DictionaryValue> config,
      bool consent,
      const DaemonController::CompletionCallback& done,
      HRESULT hr);

  // |control_| and |control2_| hold references to an instance of the daemon
  // controller to prevent a UAC prompt on every operation.
  base::win::ScopedComPtr<IDaemonControl> control_;
  base::win::ScopedComPtr<IDaemonControl2> control2_;

  // True if |control_| holds a reference to an elevated instance of the daemon
  // controller.
  bool control_is_elevated_;

  // This timer is used to release |control_| after a timeout.
  scoped_ptr<base::OneShotTimer<DaemonControllerDelegateWin> > release_timer_;

  // Handle of the plugin window.
  HWND window_handle_;

  scoped_ptr<DaemonInstallerWin> installer_;

  DISALLOW_COPY_AND_ASSIGN(DaemonControllerDelegateWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_DELEGATE_WIN_H_
