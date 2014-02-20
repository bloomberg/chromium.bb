// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_

#include "remoting/host/basic_desktop_environment.h"

namespace remoting {

class CurtainMode;
class HostWindow;
class LocalInputMonitor;

// Same as BasicDesktopEnvironment but supports desktop resizing and X DAMAGE
// notifications on Linux.
class Me2MeDesktopEnvironment : public BasicDesktopEnvironment {
 public:
  virtual ~Me2MeDesktopEnvironment();

  // DesktopEnvironment interface.
  virtual scoped_ptr<ScreenControls> CreateScreenControls() OVERRIDE;
  virtual scoped_ptr<webrtc::ScreenCapturer> CreateVideoCapturer() OVERRIDE;
  virtual std::string GetCapabilities() const OVERRIDE;
  virtual scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) OVERRIDE;

 protected:
  friend class Me2MeDesktopEnvironmentFactory;
  Me2MeDesktopEnvironment(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Initializes security features of the desktop environment (the curtain mode
  // and in-session UI).
  bool InitializeSecurity(
      base::WeakPtr<ClientSessionControl> client_session_control,
      bool curtain_enabled);

  void SetEnableGnubbyAuth(bool gnubby_auth_enabled);

 private:
  // "Curtains" the session making sure it is disconnected from the local
  // console.
  scoped_ptr<CurtainMode> curtain_;

  // Presents the disconnect window to the local user.
  scoped_ptr<HostWindow> disconnect_window_;

  // Notifies the client session about the local mouse movements.
  scoped_ptr<LocalInputMonitor> local_input_monitor_;

  // True if gnubby auth is enabled.
  bool gnubby_auth_enabled_;

  DISALLOW_COPY_AND_ASSIGN(Me2MeDesktopEnvironment);
};

// Used to create |Me2MeDesktopEnvironment| instances.
class Me2MeDesktopEnvironmentFactory : public BasicDesktopEnvironmentFactory {
 public:
  Me2MeDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~Me2MeDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory interface.
  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) OVERRIDE;
  virtual void SetEnableCurtaining(bool enable) OVERRIDE;
  virtual void SetEnableGnubbyAuth(bool enable) OVERRIDE;

 protected:
  bool curtain_enabled() const { return curtain_enabled_; }

 private:
  // True if curtain mode is enabled.
  bool curtain_enabled_;

  // True if gnubby auth is enabled.
  bool gnubby_auth_enabled_;

  DISALLOW_COPY_AND_ASSIGN(Me2MeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_
