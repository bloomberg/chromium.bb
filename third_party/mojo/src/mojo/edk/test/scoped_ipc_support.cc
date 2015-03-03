// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/scoped_ipc_support.h"

#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder.h"

namespace mojo {
namespace test {

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner)
    : io_thread_task_runner_(io_thread_task_runner),
      event_(true, false) {  // Manual reset.
  // Note: Run delegate methods on the I/O thread.
  embedder::InitIPCSupport(embedder::ProcessType::NONE, io_thread_task_runner,
                           this, io_thread_task_runner,
                           embedder::ScopedPlatformHandle());
}

ScopedIPCSupport::~ScopedIPCSupport() {
  if (base::MessageLoop::current() &&
      base::MessageLoop::current()->task_runner() == io_thread_task_runner_) {
    embedder::ShutdownIPCSupportOnIOThread();
  } else {
    embedder::ShutdownIPCSupport();
    event_.Wait();
  }
}

void ScopedIPCSupport::OnShutdownComplete() {
  event_.Signal();
}

}  // namespace test
}  // namespace mojo
