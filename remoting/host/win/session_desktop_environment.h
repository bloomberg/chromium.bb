// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/basic_desktop_environment.h"

namespace remoting {

// Used to create audio/video capturers and event executor that are compatible
// with Windows sessions.
class SessionDesktopEnvironment : public BasicDesktopEnvironment {
 public:
  explicit SessionDesktopEnvironment(const base::Closure& inject_sas);
  virtual ~SessionDesktopEnvironment();

  // DesktopEnvironment implementation.
  virtual scoped_ptr<EventExecutor> CreateEventExecutor(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) OVERRIDE;

 private:
  // Used to ask the daemon to inject Secure Attention Sequence.
  base::Closure inject_sas_;

  DISALLOW_COPY_AND_ASSIGN(SessionDesktopEnvironment);
};

// Used to create |SessionDesktopEnvironment| instances.
class SessionDesktopEnvironmentFactory : public BasicDesktopEnvironmentFactory {
 public:
  explicit SessionDesktopEnvironmentFactory(const base::Closure& inject_sas);
  virtual ~SessionDesktopEnvironmentFactory();

  // DesktopEnvironmentFactory implementation.
  virtual scoped_ptr<DesktopEnvironment> Create(
      const std::string& client_jid,
      const base::Closure& disconnect_callback) OVERRIDE;

 private:
  // Used to ask the daemon to inject Secure Attention Sequence.
  base::Closure inject_sas_;

  DISALLOW_COPY_AND_ASSIGN(SessionDesktopEnvironmentFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_DESKTOP_ENVIRONMENT_H_
