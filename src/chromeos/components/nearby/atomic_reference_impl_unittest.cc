// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/atomic_reference_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/unguessable_token.h"
#include "chromeos/components/nearby/library/atomic_reference.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

template <typename T>
class GenericAtomicReferenceImplTest : public testing::Test {
 protected:
  GenericAtomicReferenceImplTest() = default;

  void SetValueInThread(const base::Thread& thread, T value) {
    thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&GenericAtomicReferenceImplTest::SetValue,
                                  base::Unretained(this), value));
  }

  void VerifyValueInThread(const base::Thread& thread, T value) {
    thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&GenericAtomicReferenceImplTest::VerifyValue,
                                  base::Unretained(this), value));
  }

  void VerifyValue(T value) { EXPECT_EQ(value, GetValue()); }

  void SetValue(T value) { atomic_reference_->set(value); }
  T GetValue() { return atomic_reference_->get(); }

  void TinyTimeout() {
    // As of writing, tiny_timeout() is 100ms.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  std::unique_ptr<location::nearby::AtomicReference<T>> atomic_reference_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GenericAtomicReferenceImplTest);
};

class UnguessableTokenAtomicReferenceImplTest
    : public GenericAtomicReferenceImplTest<base::UnguessableToken> {
 protected:
  UnguessableTokenAtomicReferenceImplTest()
      : initial_value_(base::UnguessableToken::Create()) {}

  void SetUp() override {
    atomic_reference_ =
        std::make_unique<AtomicReferenceImpl<base::UnguessableToken>>(
            initial_value_);
  }

  base::UnguessableToken initial_value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnguessableTokenAtomicReferenceImplTest);
};

TEST_F(UnguessableTokenAtomicReferenceImplTest, GetOnSameThread) {
  VerifyValue(initial_value_);
}

TEST_F(UnguessableTokenAtomicReferenceImplTest, SetGetOnSameThread) {
  base::UnguessableToken new_token = base::UnguessableToken::Create();
  SetValue(new_token);
  VerifyValue(new_token);
}

TEST_F(UnguessableTokenAtomicReferenceImplTest, SetOnNewThread) {
  base::Thread thread("AtomicReferenceImplTest.SetOnNewThread");
  EXPECT_TRUE(thread.Start());

  base::UnguessableToken new_thread_token = base::UnguessableToken::Create();
  SetValueInThread(thread, new_thread_token);
  TinyTimeout();
  VerifyValue(new_thread_token);
}

TEST_F(UnguessableTokenAtomicReferenceImplTest, GetOnNewThread) {
  base::Thread thread("AtomicReferenceImplTest.GetOnNewThread");
  EXPECT_TRUE(thread.Start());

  base::UnguessableToken new_token = base::UnguessableToken::Create();
  SetValue(new_token);
  VerifyValueInThread(thread, new_token);
}

}  // namespace nearby

}  // namespace chromeos
