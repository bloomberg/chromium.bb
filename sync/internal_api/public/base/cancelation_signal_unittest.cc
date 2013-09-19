// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/cancelation_signal.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "sync/internal_api/public/base/cancelation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class BlockingTask : public CancelationObserver {
 public:
  BlockingTask(CancelationSignal* cancel_signal);
  virtual ~BlockingTask();

  // Starts the |exec_thread_| and uses it to execute DoRun().
  void RunAsync(base::WaitableEvent* task_done_signal);

  // Blocks until canceled.  Signals |task_done_signal| when finished.
  void Run(base::WaitableEvent* task_done_signal);

  // Implementation of CancelationObserver.
  // Wakes up the thread blocked in Run().
  virtual void OnSignalReceived() OVERRIDE;

  // Checks if we ever did successfully start waiting for |event_|.  Be careful
  // with this.  The flag itself is thread-unsafe, and the event that flips it
  // is racy.
  bool WasStarted();

 private:
  base::WaitableEvent event_;
  base::Thread exec_thread_;
  CancelationSignal* cancel_signal_;
  bool was_started_;
};

BlockingTask::BlockingTask(CancelationSignal* cancel_signal)
  : event_(true, false),
    exec_thread_("BlockingTaskBackgroundThread"),
    cancel_signal_(cancel_signal),
    was_started_(false) { }

BlockingTask::~BlockingTask() {}

void BlockingTask::RunAsync(base::WaitableEvent* task_done_signal) {
  exec_thread_.Start();
  exec_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&BlockingTask::Run,
                 base::Unretained(this),
                 base::Unretained(task_done_signal)));
}

void BlockingTask::Run(base::WaitableEvent* task_done_signal) {
  if (cancel_signal_->TryRegisterHandler(this)) {
    DCHECK(!event_.IsSignaled());
    was_started_ = true;
    event_.Wait();
  }
  task_done_signal->Signal();
}

void BlockingTask::OnSignalReceived() {
  event_.Signal();
}

bool BlockingTask::WasStarted() {
  return was_started_;
}

class CancelationSignalTest : public ::testing::Test {
 public:
  CancelationSignalTest();
  virtual ~CancelationSignalTest();

  // Starts the blocking task on a background thread.
  void StartBlockingTask();

  // Cancels the blocking task.
  void CancelBlocking();

  // Verifies that the background task is not running.  This could be beacause
  // it was canceled early or because it was canceled after it was started.
  //
  // This method may block for a brief period of time while waiting for the
  // background thread to make progress.
  bool VerifyTaskDone();

  // Verifies that the background task was canceled early.
  //
  // This method may block for a brief period of time while waiting for the
  // background thread to make progress.
  bool VerifyTaskNotStarted();

 private:
  base::MessageLoop main_loop_;

  CancelationSignal signal_;
  base::WaitableEvent task_done_event_;
  BlockingTask blocking_task_;
};

CancelationSignalTest::CancelationSignalTest()
  : task_done_event_(false, false), blocking_task_(&signal_) {}

CancelationSignalTest::~CancelationSignalTest() {}

void CancelationSignalTest::StartBlockingTask() {
  blocking_task_.RunAsync(&task_done_event_);
}

void CancelationSignalTest::CancelBlocking() {
  signal_.Signal();
}

bool CancelationSignalTest::VerifyTaskDone() {
  // Wait until BlockingTask::Run() has finished.
  task_done_event_.Wait();
  return true;
}

bool CancelationSignalTest::VerifyTaskNotStarted() {
  // Wait until BlockingTask::Run() has finished.
  task_done_event_.Wait();

  // Verify the background thread never started blocking.
  return !blocking_task_.WasStarted();
}

class FakeCancelationObserver : public CancelationObserver {
  virtual void OnSignalReceived() OVERRIDE { }
};

TEST(CancelationSignalTest_SingleThread, CheckFlags) {
  FakeCancelationObserver observer;
  CancelationSignal signal;

  EXPECT_FALSE(signal.IsSignalled());
  signal.Signal();
  EXPECT_TRUE(signal.IsSignalled());
  EXPECT_FALSE(signal.TryRegisterHandler(&observer));
}

// Send the cancelation signal before the task is started.  This will ensure
// that the task will never be attempted.
TEST_F(CancelationSignalTest, CancelEarly) {
  CancelBlocking();
  StartBlockingTask();
  EXPECT_TRUE(VerifyTaskNotStarted());
}

// Send the cancelation signal after the request to start the task has been
// posted.  This is racy.  The signal to stop may arrive before the signal to
// run the task.  If that happens, we end up with another instance of the
// CancelEarly test defined earlier.  If the signal requesting a stop arrives
// after the task has been started, it should end up stopping the task.
TEST_F(CancelationSignalTest, Cancel) {
  StartBlockingTask();
  CancelBlocking();
  EXPECT_TRUE(VerifyTaskDone());
}

}  // namespace syncer
