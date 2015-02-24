// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/thread_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "sandbox/linux/services/proc_util.h"

namespace sandbox {

namespace {

const char kAssertSingleThreadedError[] =
    "Current process is not mono-threaded!";

bool IsSingleThreadedImpl(int proc_fd) {
  CHECK_LE(0, proc_fd);
  struct stat task_stat;
  int fstat_ret = fstatat(proc_fd, "self/task/", &task_stat, 0);
  PCHECK(0 == fstat_ret);

  // At least "..", "." and the current thread should be present.
  CHECK_LE(3UL, task_stat.st_nlink);
  // Counting threads via /proc/self/task could be racy. For the purpose of
  // determining if the current proces is monothreaded it works: if at any
  // time it becomes monothreaded, it'll stay so.
  return task_stat.st_nlink == 3;
}

bool IsThreadPresentInProcFS(int proc_fd,
                             const std::string& thread_id_dir_str) {
  struct stat task_stat;
  const int fstat_ret =
      fstatat(proc_fd, thread_id_dir_str.c_str(), &task_stat, 0);
  if (fstat_ret < 0) {
    PCHECK(ENOENT == errno);
    return false;
  }
  return true;
}

// Run |cb| in a loop until it returns false. Every time |cb| runs, sleep
// for an exponentially increasing amount of time. |cb| is expected to return
// false very quickly and this will crash if it doesn't happen within ~64ms on
// Debug builds (2s on Release builds).
// This is guaranteed to not sleep more than twice as much as the bare minimum
// amount of time.
void RunWhileTrue(const base::Callback<bool(void)>& cb) {
#if defined(NDEBUG)
  // In Release mode, crash after 30 iterations, which means having spent
  // roughly 2s in
  // nanosleep(2) cumulatively.
  const unsigned int kMaxIterations = 30U;
#else
  // In practice, this never goes through more than a couple iterations. In
  // debug mode, crash after 64ms (+ eventually 25 times the granularity of
  // the clock) in nanosleep(2). This ensures that this is not becoming too
  // slow.
  const unsigned int kMaxIterations = 25U;
#endif

  // Run |cb| with an exponential back-off, sleeping 2^iterations nanoseconds
  // in nanosleep(2).
  // Note: the clock may not allow for nanosecond granularity, in this case the
  // first iterations would sleep a tiny bit more instead, which would not
  // change the calculations significantly.
  for (unsigned int i = 0; i < kMaxIterations; ++i) {
    if (!cb.Run()) {
      return;
    }

    // Increase the waiting time exponentially.
    struct timespec ts = {0, 1L << i /* nanoseconds */};
    PCHECK(0 == HANDLE_EINTR(nanosleep(&ts, &ts)));
  }

  LOG(FATAL) << kAssertSingleThreadedError << " (iterations: " << kMaxIterations
             << ")";

  NOTREACHED();
}

bool IsMultiThreaded(int proc_fd) {
  return !ThreadHelpers::IsSingleThreaded(proc_fd);
}

}  // namespace

// static
bool ThreadHelpers::IsSingleThreaded(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  return IsSingleThreadedImpl(proc_fd);
}

// static
bool ThreadHelpers::IsSingleThreaded() {
  base::ScopedFD task_fd(ProcUtil::OpenProc());
  return IsSingleThreaded(task_fd.get());
}

// static
void ThreadHelpers::AssertSingleThreaded(int proc_fd) {
  DCHECK_LE(0, proc_fd);
  const base::Callback<bool(void)> cb = base::Bind(&IsMultiThreaded, proc_fd);
  RunWhileTrue(cb);
}

void ThreadHelpers::AssertSingleThreaded() {
  base::ScopedFD task_fd(ProcUtil::OpenProc());
  AssertSingleThreaded(task_fd.get());
}

// static
bool ThreadHelpers::StopThreadAndWatchProcFS(int proc_fd,
                                             base::Thread* thread) {
  DCHECK_LE(0, proc_fd);
  DCHECK(thread);
  const base::PlatformThreadId thread_id = thread->thread_id();
  const std::string thread_id_dir_str =
      "self/task/" + base::IntToString(thread_id) + "/";

  // The kernel is at liberty to wake the thread id futex before updating
  // /proc. Following Stop(), the thread is joined, but entries in /proc may
  // not have been updated.
  thread->Stop();

  const base::Callback<bool(void)> cb =
      base::Bind(&IsThreadPresentInProcFS, proc_fd, thread_id_dir_str);

  RunWhileTrue(cb);

  return true;
}

// static
const char* ThreadHelpers::GetAssertSingleThreadedErrorMessageForTests() {
  return kAssertSingleThreadedError;
}

}  // namespace sandbox
