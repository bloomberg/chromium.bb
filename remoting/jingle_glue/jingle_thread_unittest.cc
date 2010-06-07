// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class MockTask : public Task {
 public:
  MOCK_METHOD0(Run, void());
};

TEST(JingleThreadTest, PostTask) {
  JingleThread thread;
  MockTask* task = new MockTask();
  EXPECT_CALL(*task, Run());

  thread.Start();
  thread.message_loop()->PostTask(FROM_HERE, task);
  thread.Stop();
}

}  // namespace remoting
