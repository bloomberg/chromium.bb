// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jamiewalch): Add unit tests for this.

#include "remoting/host/sighup_listener_mac.h"

#include <errno.h>

#include "base/compiler_specific.h"
#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/message_pump_libevent.h"
#include "base/threading/platform_thread.h"

namespace {

class SigHupListener : public base::MessagePumpLibevent::Watcher {
 public:
  SigHupListener(const base::Closure& callback);

  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}

  // WatchFileDescriptor needs a controller through which the operation can be
  // canceled. We don't use it, but this is as good a place as any to store it.
  base::MessagePumpLibevent::FileDescriptorWatcher controller;

 private:
  base::Closure callback_;
};

SigHupListener::SigHupListener(const base::Closure& callback)
    : callback_(callback) {
}

void SigHupListener::OnFileCanReadWithoutBlocking(int fd) {
  char buffer;
  int result = HANDLE_EINTR(read(fd, &buffer, sizeof(buffer)));
  if (result > 0) {
    callback_.Run();
  }
}

SigHupListener* g_config_updater = NULL;
int g_write_fd = 0;

void HupSignalHandler(int signal) {
  int r ALLOW_UNUSED = write(g_write_fd, "", 1);
}

}  // namespace

namespace remoting {

// RegisterHupSignalHandler registers a signal handler that writes a byte to a
// pipe each time SIGHUP is received. The read end of the pipe is registered
// with the current MessageLoop (which must be of type IO); whenever the pipe
// is readable, it invokes the specified callback.
//
// This arrangement is required because the set of system APIs that are safe to
// call from a singal handler is very limited (but does include write).
bool RegisterHupSignalHandler(const base::Closure& callback) {
  DCHECK(!g_config_updater);
  MessageLoopForIO* message_loop = MessageLoopForIO::current();
  int pipefd[2];
  int result = pipe(pipefd);
  if (result < 0) {
    LOG(ERROR) << "Could not create SIGHUP pipe: " << errno;
    return false;
  }
  g_write_fd = pipefd[1];
  g_config_updater = new SigHupListener(callback);
  result = message_loop->WatchFileDescriptor(
      pipefd[0], true, MessageLoopForIO::WATCH_READ,
      &g_config_updater->controller, g_config_updater);
  if (!result) {
    delete g_config_updater;
    g_config_updater = NULL;
    LOG(ERROR) << "Failed to create SIGHUP detector task.";
    return false;
  }
  if (signal(SIGHUP, HupSignalHandler) == SIG_ERR) {
    delete g_config_updater;
    g_config_updater = NULL;
    LOG(ERROR) << "signal() failed: " << errno;
    return false;
  }
  return true;
}

}  // namespace remoting
