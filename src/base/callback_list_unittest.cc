// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Listener {
 public:
  Listener() = default;
  explicit Listener(int scaler) : scaler_(scaler) {}
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  ~Listener() = default;

  void IncrementTotal() { ++total_; }

  void IncrementByMultipleOfScaler(int x) { total_ += x * scaler_; }

  int total() const { return total_; }

 private:
  int total_ = 0;
  int scaler_ = 1;
};

template <typename T>
class Remover {
 public:
  Remover() = default;
  Remover(const Remover&) = delete;
  Remover& operator=(const Remover&) = delete;
  ~Remover() = default;

  void IncrementTotalAndRemove() {
    ++total_;
    removal_subscription_.reset();
  }

  void SetSubscriptionToRemove(std::unique_ptr<typename T::Subscription> sub) {
    removal_subscription_ = std::move(sub);
  }

  int total() const { return total_; }

 private:
  int total_ = 0;
  std::unique_ptr<typename T::Subscription> removal_subscription_;
};

class Adder {
 public:
  explicit Adder(RepeatingClosureList* cb_reg) : cb_reg_(cb_reg) {}
  Adder(const Adder&) = delete;
  Adder& operator=(const Adder&) = delete;
  ~Adder() = default;

  void AddCallback() {
    if (!added_) {
      added_ = true;
      subscription_ =
          cb_reg_->Add(BindRepeating(&Adder::IncrementTotal, Unretained(this)));
    }
  }

  void IncrementTotal() { ++total_; }

  bool added() const { return added_; }
  int total() const { return total_; }

 private:
  bool added_ = false;
  int total_ = 0;
  RepeatingClosureList* cb_reg_;
  std::unique_ptr<RepeatingClosureList::Subscription> subscription_;
};

class Summer {
 public:
  Summer() = default;
  Summer(const Summer&) = delete;
  Summer& operator=(const Summer&) = delete;
  ~Summer() = default;

  void AddOneParam(int a) { value_ = a; }
  void AddTwoParam(int a, int b) { value_ = a + b; }
  void AddThreeParam(int a, int b, int c) { value_ = a + b + c; }
  void AddFourParam(int a, int b, int c, int d) { value_ = a + b + c + d; }
  void AddFiveParam(int a, int b, int c, int d, int e) {
    value_ = a + b + c + d + e;
  }
  void AddSixParam(int a, int b, int c, int d, int e , int f) {
    value_ = a + b + c + d + e + f;
  }

  int value() const { return value_; }

 private:
  int value_ = 0;
};

class Counter {
 public:
  Counter() = default;
  Counter(const Counter&) = delete;
  Counter& operator=(const Counter&) = delete;
  ~Counter() = default;

  void Increment() { ++value_; }

  int value() const { return value_; }

 private:
  int value_ = 0;
};

// Sanity check that we can instantiate a CallbackList for each arity.
TEST(CallbackListTest, ArityTest) {
  Summer s;

  RepeatingCallbackList<void(int)> c1;
  std::unique_ptr<RepeatingCallbackList<void(int)>::Subscription>
      subscription1 =
          c1.Add(BindRepeating(&Summer::AddOneParam, Unretained(&s)));

  c1.Notify(1);
  EXPECT_EQ(1, s.value());

  RepeatingCallbackList<void(int, int)> c2;
  std::unique_ptr<RepeatingCallbackList<void(int, int)>::Subscription>
      subscription2 =
          c2.Add(BindRepeating(&Summer::AddTwoParam, Unretained(&s)));

  c2.Notify(1, 2);
  EXPECT_EQ(3, s.value());

  RepeatingCallbackList<void(int, int, int)> c3;
  std::unique_ptr<RepeatingCallbackList<void(int, int, int)>::Subscription>
      subscription3 =
          c3.Add(BindRepeating(&Summer::AddThreeParam, Unretained(&s)));

  c3.Notify(1, 2, 3);
  EXPECT_EQ(6, s.value());

  RepeatingCallbackList<void(int, int, int, int)> c4;
  std::unique_ptr<RepeatingCallbackList<void(int, int, int, int)>::Subscription>
      subscription4 =
          c4.Add(BindRepeating(&Summer::AddFourParam, Unretained(&s)));

  c4.Notify(1, 2, 3, 4);
  EXPECT_EQ(10, s.value());

  RepeatingCallbackList<void(int, int, int, int, int)> c5;
  std::unique_ptr<
      RepeatingCallbackList<void(int, int, int, int, int)>::Subscription>
      subscription5 =
          c5.Add(BindRepeating(&Summer::AddFiveParam, Unretained(&s)));

  c5.Notify(1, 2, 3, 4, 5);
  EXPECT_EQ(15, s.value());

  RepeatingCallbackList<void(int, int, int, int, int, int)> c6;
  std::unique_ptr<
      RepeatingCallbackList<void(int, int, int, int, int, int)>::Subscription>
      subscription6 =
          c6.Add(BindRepeating(&Summer::AddSixParam, Unretained(&s)));

  c6.Notify(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(21, s.value());
}

// Sanity check that closures added to the list will be run, and those removed
// from the list will not be run.
TEST(CallbackListTest, BasicTest) {
  RepeatingClosureList cb_reg;
  Listener a, b, c;

  std::unique_ptr<RepeatingClosureList::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&a)));
  std::unique_ptr<RepeatingClosureList::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());

  b_subscription.reset();

  std::unique_ptr<RepeatingClosureList::Subscription> c_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&c)));

  cb_reg.Notify();

  EXPECT_EQ(2, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_EQ(1, c.total());
}

// Similar to BasicTest but with OnceCallbacks instead of Repeating.
TEST(CallbackListTest, OnceCallbacks) {
  OnceClosureList cb_reg;
  Listener a, b, c;

  std::unique_ptr<OnceClosureList::Subscription> a_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&a)));
  std::unique_ptr<OnceClosureList::Subscription> b_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());

  // OnceCallbacks should auto-remove themselves after calling Notify().
  EXPECT_TRUE(cb_reg.empty());

  // Destroying a subscription after the callback is canceled should not cause
  // any problems.
  b_subscription.reset();

  std::unique_ptr<OnceClosureList::Subscription> c_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&c)));

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_EQ(1, c.total());
}

// Sanity check that callbacks with details added to the list will be run, with
// the correct details, and those removed from the list will not be run.
TEST(CallbackListTest, BasicTestWithParams) {
  using CallbackListType = RepeatingCallbackList<void(int)>;
  CallbackListType cb_reg;
  Listener a(1), b(-1), c(1);

  std::unique_ptr<CallbackListType::Subscription> a_subscription = cb_reg.Add(
      BindRepeating(&Listener::IncrementByMultipleOfScaler, Unretained(&a)));
  std::unique_ptr<CallbackListType::Subscription> b_subscription = cb_reg.Add(
      BindRepeating(&Listener::IncrementByMultipleOfScaler, Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify(10);

  EXPECT_EQ(10, a.total());
  EXPECT_EQ(-10, b.total());

  b_subscription.reset();

  std::unique_ptr<CallbackListType::Subscription> c_subscription = cb_reg.Add(
      BindRepeating(&Listener::IncrementByMultipleOfScaler, Unretained(&c)));

  cb_reg.Notify(10);

  EXPECT_EQ(20, a.total());
  EXPECT_EQ(-10, b.total());
  EXPECT_EQ(10, c.total());
}

// Test the a callback can remove itself or a different callback from the list
// during iteration without invalidating the iterator.
TEST(CallbackListTest, RemoveCallbacksDuringIteration) {
  RepeatingClosureList cb_reg;
  Listener a, b;
  Remover<RepeatingClosureList> remover_1, remover_2;

  std::unique_ptr<RepeatingClosureList::Subscription> remover_1_sub =
      cb_reg.Add(
          BindRepeating(&Remover<RepeatingClosureList>::IncrementTotalAndRemove,
                        Unretained(&remover_1)));
  std::unique_ptr<RepeatingClosureList::Subscription> remover_2_sub =
      cb_reg.Add(
          BindRepeating(&Remover<RepeatingClosureList>::IncrementTotalAndRemove,
                        Unretained(&remover_2)));
  std::unique_ptr<RepeatingClosureList::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&a)));
  std::unique_ptr<RepeatingClosureList::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  // |remover_1| will remove itself.
  remover_1.SetSubscriptionToRemove(std::move(remover_1_sub));
  // |remover_2| will remove a.
  remover_2.SetSubscriptionToRemove(std::move(a_subscription));

  cb_reg.Notify();

  // |remover_1| runs once (and removes itself), |remover_2| runs once (and
  // removes a), |a| never runs, and |b| runs once.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(1, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(1, b.total());

  cb_reg.Notify();

  // Only |remover_2| and |b| run this time.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(2, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(2, b.total());
}

// Similar to RemoveCallbacksDuringIteration but with OnceCallbacks instead of
// Repeating.
TEST(CallbackListTest, RemoveOnceCallbacksDuringIteration) {
  OnceClosureList cb_reg;
  Listener a, b;
  Remover<OnceClosureList> remover_1, remover_2;

  std::unique_ptr<OnceClosureList::Subscription> remover_1_sub =
      cb_reg.Add(BindOnce(&Remover<OnceClosureList>::IncrementTotalAndRemove,
                          Unretained(&remover_1)));
  std::unique_ptr<OnceClosureList::Subscription> remover_2_sub =
      cb_reg.Add(BindOnce(&Remover<OnceClosureList>::IncrementTotalAndRemove,
                          Unretained(&remover_2)));
  std::unique_ptr<OnceClosureList::Subscription> a_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&a)));
  std::unique_ptr<OnceClosureList::Subscription> b_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&b)));

  // |remover_1| will remove itself.
  remover_1.SetSubscriptionToRemove(std::move(remover_1_sub));
  // |remover_2| will remove a.
  remover_2.SetSubscriptionToRemove(std::move(a_subscription));

  cb_reg.Notify();

  // |remover_1| runs once (and removes itself), |remover_2| runs once (and
  // removes a), |a| never runs, and |b| runs once.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(1, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(1, b.total());

  cb_reg.Notify();

  // Nothing runs this time.
  EXPECT_EQ(1, remover_1.total());
  EXPECT_EQ(1, remover_2.total());
  EXPECT_EQ(0, a.total());
  EXPECT_EQ(1, b.total());
}

// Test that a callback can add another callback to the list durning iteration
// without invalidating the iterator. The newly added callback should be run on
// the current iteration as will all other callbacks in the list.
TEST(CallbackListTest, AddCallbacksDuringIteration) {
  RepeatingClosureList cb_reg;
  Adder a(&cb_reg);
  Listener b;
  std::unique_ptr<RepeatingClosureList::Subscription> a_subscription =
      cb_reg.Add(BindRepeating(&Adder::AddCallback, Unretained(&a)));
  std::unique_ptr<RepeatingClosureList::Subscription> b_subscription =
      cb_reg.Add(BindRepeating(&Listener::IncrementTotal, Unretained(&b)));

  cb_reg.Notify();

  EXPECT_EQ(1, a.total());
  EXPECT_EQ(1, b.total());
  EXPECT_TRUE(a.added());

  cb_reg.Notify();

  EXPECT_EQ(2, a.total());
  EXPECT_EQ(2, b.total());
}

// Sanity check: notifying an empty list is a no-op.
TEST(CallbackListTest, EmptyList) {
  RepeatingClosureList cb_reg;

  cb_reg.Notify();
}

TEST(CallbackListTest, RemovalCallback) {
  Counter remove_count;
  RepeatingClosureList cb_reg;
  cb_reg.set_removal_callback(
      BindRepeating(&Counter::Increment, Unretained(&remove_count)));

  std::unique_ptr<RepeatingClosureList::Subscription> subscription =
      cb_reg.Add(DoNothing());

  // Removing a subscription outside of iteration signals the callback.
  EXPECT_EQ(0, remove_count.value());
  subscription.reset();
  EXPECT_EQ(1, remove_count.value());

  // Configure two subscriptions to remove themselves.
  Remover<RepeatingClosureList> remover_1, remover_2;
  std::unique_ptr<RepeatingClosureList::Subscription> remover_1_sub =
      cb_reg.Add(
          BindRepeating(&Remover<RepeatingClosureList>::IncrementTotalAndRemove,
                        Unretained(&remover_1)));
  std::unique_ptr<RepeatingClosureList::Subscription> remover_2_sub =
      cb_reg.Add(
          BindRepeating(&Remover<RepeatingClosureList>::IncrementTotalAndRemove,
                        Unretained(&remover_2)));
  remover_1.SetSubscriptionToRemove(std::move(remover_1_sub));
  remover_2.SetSubscriptionToRemove(std::move(remover_2_sub));

  // The callback should be signaled exactly once.
  EXPECT_EQ(1, remove_count.value());
  cb_reg.Notify();
  EXPECT_EQ(2, remove_count.value());
  EXPECT_TRUE(cb_reg.empty());
}

TEST(CallbackListTest, AbandonSubscriptions) {
  Listener listener;
  std::unique_ptr<RepeatingClosureList::Subscription> subscription;
  {
    RepeatingClosureList cb_reg;
    subscription = cb_reg.Add(
        BindRepeating(&Listener::IncrementTotal, Unretained(&listener)));
    // Make sure the callback is signaled while cb_reg is in scope.
    cb_reg.Notify();
    // Exiting this scope and running the cb_reg destructor shouldn't fail.
  }
  EXPECT_EQ(1, listener.total());

  // Destroying the subscription after the list should not cause any problems.
  subscription.reset();
}

TEST(CallbackListTest, CancelBeforeRunning) {
  OnceClosureList cb_reg;
  Listener a;

  std::unique_ptr<OnceClosureList::Subscription> a_subscription =
      cb_reg.Add(BindOnce(&Listener::IncrementTotal, Unretained(&a)));

  EXPECT_TRUE(a_subscription.get());

  // Canceling a OnceCallback before running it should not cause problems.
  a_subscription.reset();
  cb_reg.Notify();

  // |a| should not have received any callbacks.
  EXPECT_EQ(0, a.total());
}

}  // namespace
}  // namespace base
