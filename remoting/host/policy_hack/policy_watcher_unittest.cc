// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/host/policy_hack/fake_policy_watcher.h"
#include "remoting/host/policy_hack/mock_policy_callback.h"
#include "remoting/host/policy_hack/policy_watcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace policy_hack {

class PolicyWatcherTest : public testing::Test {
 public:
  PolicyWatcherTest() {
  }

  virtual void SetUp() OVERRIDE {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    policy_callback_ = base::Bind(&MockPolicyCallback::OnPolicyUpdate,
                                  base::Unretained(&mock_policy_callback_));
    policy_watcher_.reset(new FakePolicyWatcher(message_loop_proxy_));
    nat_true.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    nat_false.SetBoolean(PolicyWatcher::kNatPolicyName, false);
    nat_one.SetInteger(PolicyWatcher::kNatPolicyName, 1);
  }

 protected:
  void StartWatching() {
    policy_watcher_->StartWatching(policy_callback_);
    message_loop_.RunUntilIdle();
  }

  void StopWatching() {
    base::WaitableEvent stop_event(false, false);
    policy_watcher_->StopWatching(&stop_event);
    message_loop_.RunUntilIdle();
    EXPECT_EQ(true, stop_event.IsSignaled());
  }

  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockPolicyCallback mock_policy_callback_;
  PolicyWatcher::PolicyCallback policy_callback_;
  scoped_ptr<FakePolicyWatcher> policy_watcher_;
  base::DictionaryValue empty;
  base::DictionaryValue nat_true;
  base::DictionaryValue nat_false;
  base::DictionaryValue nat_one;
};

MATCHER_P(IsPolicies, dict, "") {
  return arg->Equals(dict);
}

TEST_F(PolicyWatcherTest, None) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatTrue) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_true);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatFalse) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_false)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_false);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatOne) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_false)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_one);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrue) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty);
  policy_watcher_->SetPolicies(&nat_true);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrue) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty);
  policy_watcher_->SetPolicies(&nat_true);
  policy_watcher_->SetPolicies(&nat_true);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrueThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_false)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty);
  policy_watcher_->SetPolicies(&nat_true);
  policy_watcher_->SetPolicies(&nat_true);
  policy_watcher_->SetPolicies(&nat_false);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_false)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty);
  policy_watcher_->SetPolicies(&nat_false);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenFalseThenTrue) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_false)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty);
  policy_watcher_->SetPolicies(&nat_false);
  policy_watcher_->SetPolicies(&nat_true);
  StopWatching();
}

}  // namespace policy_hack
}  // namespace remoting
