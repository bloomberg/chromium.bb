// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_TEST_PREDICATE_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_TEST_PREDICATE_WAITER_H_

#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"

namespace ash {
namespace test {

// Waits for predicate to be fulfilled.
class TestPredicateWaiter : public TestConditionWaiter {
 public:
  using PredicateCheck = base::RepeatingCallback<bool(void)>;

  explicit TestPredicateWaiter(const PredicateCheck& is_fulfilled);

  TestPredicateWaiter(const TestPredicateWaiter&) = delete;
  TestPredicateWaiter& operator=(const TestPredicateWaiter&) = delete;

  ~TestPredicateWaiter() override;

  void set_description(const std::string& description) {
    description_ = description;
  }

  // TestConditionWaiter
  void Wait() override;

 private:
  void CheckPredicate();

  const PredicateCheck is_fulfilled_;

  base::RetainingOneShotTimer timer_;
  base::RunLoop run_loop_;
  std::string description_;
};

}  // namespace test
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace test {
using ::ash::test::TestPredicateWaiter;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_TEST_PREDICATE_WAITER_H_
