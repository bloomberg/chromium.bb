// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_

#include "remoting/host/basic_desktop_environment.h"

namespace remoting {

// Same as BasicDesktopEnvironment but supports desktop resizing and X DAMAGE
// notifications on Linux.
class Me2MeDesktopEnvironment : public BasicDesktopEnvironment {
 public:
  virtual ~Me2MeDesktopEnvironment();

  // DesktopEnvironment interface.
  virtual scoped_ptr<SessionController> CreateSessionController() OVERRIDE;
  virtual scoped_ptr<media::ScreenCapturer> CreateVideoCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) OVERRIDE;

 protected:
  friend class Me2MeDesktopEnvironmentFactory;
  Me2MeDesktopEnvironment();

 private:
  DISALLOW_COPY_AND_ASSIGN(Me2MeDesktopEnvironment);
};

// Used to create |Me2MeDesktopEnvironment| instances.
class Me2MeDesktopEnvironmentFactory : public BasicDesktopEnvironmentFactory {
 public:
  Me2MeDesktopEnvironmentFactory();
  virtual ~Me2MeDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory interface.
  virtual scoped_ptr<DesktopEnvironment> Create(
      const std::string& client_jid,
      const base::Closure& disconnect_callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(Me2MeDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ME2ME_DESKTOP_ENVIRONMENT_H_
