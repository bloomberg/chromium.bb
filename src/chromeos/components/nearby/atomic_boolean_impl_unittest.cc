// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/atomic_boolean_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "chromeos/components/nearby/library/atomic_boolean.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

class AtomicBooleanImplTest : public testing::Test {
 protected:
  AtomicBooleanImplTest()
      : atomic_boolean_(std::make_unique<AtomicBooleanImpl>()) {}

  void SetAtomicBooleanOnThread(const base::Thread& thread, bool value) {
    thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AtomicBooleanImplTest::SetAtomicBoolean,
                                  base::Unretained(this), value));
  }

  void VerifyAtomicBooleanOnThread(const base::Thread& thread, bool value) {
    thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AtomicBooleanImplTest::VerifyAtomicBoolean,
                                  base::Unretained(this), value));
  }

  void VerifyAtomicBoolean(bool value) { EXPECT_EQ(value, GetAtomicBoolean()); }

  void SetAtomicBoolean(bool value) { atomic_boolean_->set(value); }
  bool GetAtomicBoolean() { return atomic_boolean_->get(); }

  void TinyTimeout() {
    // As of writing, tiny_timeout() is 100ms.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

 private:
  std::unique_ptr<location::nearby::AtomicBoolean> atomic_boolean_;

  DISALLOW_COPY_AND_ASSIGN(AtomicBooleanImplTest);
};

TEST_F(AtomicBooleanImplTest, SetGetOnSameThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);
}

TEST_F(AtomicBooleanImplTest, MultipleSetGetOnSameThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);

  SetAtomicBoolean(false);
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  VerifyAtomicBoolean(true);
}

TEST_F(AtomicBooleanImplTest, SetOnNewThread) {
  VerifyAtomicBoolean(false);

  base::Thread thread("AtomicBooleanImplTest.SetOnNewThread");
  EXPECT_TRUE(thread.Start());

  SetAtomicBooleanOnThread(thread, true);
  TinyTimeout();
  VerifyAtomicBoolean(true);
}

TEST_F(AtomicBooleanImplTest, GetOnNewThread) {
  VerifyAtomicBoolean(false);

  SetAtomicBoolean(true);
  EXPECT_TRUE(GetAtomicBoolean());

  base::Thread thread("AtomicBooleanImplTest.GetOnNewThread");
  EXPECT_TRUE(thread.Start());

  VerifyAtomicBooleanOnThread(thread, true);
}

}  // namespace nearby

}  // namespace chromeos
