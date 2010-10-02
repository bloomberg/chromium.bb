// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class MockTask : public Task {
 public:
  MOCK_METHOD0(Run, void());
};

namespace {
// Delay used to test delayed tasks. Shouldn't be too big, so that we don't
// slow down the test, yet, should be big enough to be measurable.
int kDelayMs = 50;  // 0.05 s.
int kDelayTimeoutMs = 10000;  // 10 s.
}  // namespace

TEST(JingleThreadTest, PostTask) {
  JingleThread thread;
  MockTask* task = new MockTask();
  EXPECT_CALL(*task, Run());

  thread.Start();
  thread.message_loop()->PostTask(FROM_HERE, task);
  thread.Stop();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

TEST(JingleThreadTest, PostDelayedTask) {
  JingleThread thread;
  MockTask* task = new MockTask();
  base::WaitableEvent event(true, false);
  EXPECT_CALL(*task, Run()).WillOnce(SignalEvent(&event));

  thread.Start();
  base::Time start = base::Time::Now();
  thread.message_loop()->PostDelayedTask(FROM_HERE, task, kDelayMs);
  event.TimedWait(base::TimeDelta::FromMilliseconds(kDelayTimeoutMs));
  base::Time end = base::Time::Now();
  thread.Stop();

  EXPECT_GE((end - start).InMillisecondsRoundedUp(), kDelayMs);
}

}  // namespace remoting
