// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_FACTORY_H_
#define REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_FACTORY_H_

#include "base/callback.h"
#include "remoting/host/desktop_environment_factory.h"

namespace remoting {

class SessionDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  SessionDesktopEnvironmentFactory(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const base::Closure& inject_sas);
  virtual ~SessionDesktopEnvironmentFactory();

  virtual scoped_ptr<DesktopEnvironment> Create() OVERRIDE;

 private:
  // Used to ask the daemon to inject Secure Attention Sequence.
  base::Closure inject_sas_;

  DISALLOW_COPY_AND_ASSIGN(SessionDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_FACTORY_H_
