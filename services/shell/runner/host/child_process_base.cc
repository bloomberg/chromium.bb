// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/host/child_process_base.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "services/shell/runner/common/client_util.h"

namespace shell {

namespace {

// Should be created and initialized on the main thread and kept alive as long
// a Mojo application is running in the current process.
class ScopedAppContext : public mojo::edk::ProcessDelegate {
 public:
  ScopedAppContext()
      : io_thread_("io_thread"), wait_for_shutdown_event_(true, false) {
    // Initialize Mojo before starting any threads.
    mojo::edk::Init();

    // Create and start our I/O thread.
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(io_thread_options));
    io_runner_ = io_thread_.task_runner().get();
    CHECK(io_runner_.get());

    mojo::edk::InitIPCSupport(this, io_runner_);
    mojo::edk::SetParentPipeHandleFromCommandLine();
  }

  ~ScopedAppContext() override {
    mojo::edk::ShutdownIPCSupport();
    wait_for_shutdown_event_.Wait();
  }

 private:
  // ProcessDelegate implementation.
  void OnShutdownComplete() override {
    wait_for_shutdown_event_.Signal();
  }

  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  // Used to unblock the main thread on shutdown.
  base::WaitableEvent wait_for_shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppContext);
};

}  // namespace

void ChildProcessMain(const RunCallback& callback) {
  DCHECK(!base::MessageLoop::current());

  ScopedAppContext app_context;
  callback.Run(GetShellClientRequestFromCommandLine());
}

}  // namespace shell
