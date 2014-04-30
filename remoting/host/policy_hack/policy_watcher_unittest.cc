// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/host/dns_blackhole_checker.h"
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
    nat_true_.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    nat_false_.SetBoolean(PolicyWatcher::kNatPolicyName, false);
    nat_one_.SetInteger(PolicyWatcher::kNatPolicyName, 1);
    domain_empty_.SetString(PolicyWatcher::kHostDomainPolicyName,
                            std::string());
    domain_full_.SetString(PolicyWatcher::kHostDomainPolicyName, kHostDomain);
    SetDefaults(nat_true_others_default_);
    nat_true_others_default_.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    SetDefaults(nat_false_others_default_);
    nat_false_others_default_.SetBoolean(PolicyWatcher::kNatPolicyName, false);
    SetDefaults(domain_empty_others_default_);
    domain_empty_others_default_.SetString(PolicyWatcher::kHostDomainPolicyName,
                                           std::string());
    SetDefaults(domain_full_others_default_);
    domain_full_others_default_.SetString(PolicyWatcher::kHostDomainPolicyName,
                                          kHostDomain);
    nat_true_domain_empty_.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    nat_true_domain_empty_.SetString(PolicyWatcher::kHostDomainPolicyName,
                                     std::string());
    nat_true_domain_full_.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    nat_true_domain_full_.SetString(PolicyWatcher::kHostDomainPolicyName,
                                   kHostDomain);
    nat_false_domain_empty_.SetBoolean(PolicyWatcher::kNatPolicyName, false);
    nat_false_domain_empty_.SetString(PolicyWatcher::kHostDomainPolicyName,
                                      std::string());
    nat_false_domain_full_.SetBoolean(PolicyWatcher::kNatPolicyName, false);
    nat_false_domain_full_.SetString(PolicyWatcher::kHostDomainPolicyName,
                                    kHostDomain);
    SetDefaults(nat_true_domain_empty_others_default_);
    nat_true_domain_empty_others_default_.SetBoolean(
        PolicyWatcher::kNatPolicyName, true);
    nat_true_domain_empty_others_default_.SetString(
        PolicyWatcher::kHostDomainPolicyName, std::string());
    unknown_policies_.SetString("UnknownPolicyOne", std::string());
    unknown_policies_.SetString("UnknownPolicyTwo", std::string());

    const char kOverrideNatTraversalToFalse[] =
      "{ \"RemoteAccessHostFirewallTraversal\": false }";
    nat_true_and_overridden_.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    nat_true_and_overridden_.SetString(
        PolicyWatcher::kHostDebugOverridePoliciesName,
        kOverrideNatTraversalToFalse);
    pairing_true_.SetBoolean(PolicyWatcher::kHostAllowClientPairing, true);
    pairing_false_.SetBoolean(PolicyWatcher::kHostAllowClientPairing, false);
    gnubby_auth_true_.SetBoolean(PolicyWatcher::kHostAllowGnubbyAuthPolicyName,
                                 true);
    gnubby_auth_false_.SetBoolean(PolicyWatcher::kHostAllowGnubbyAuthPolicyName,
                                 false);
    relay_true_.SetBoolean(PolicyWatcher::kRelayPolicyName, true);
    relay_false_.SetBoolean(PolicyWatcher::kRelayPolicyName, false);
    port_range_full_.SetString(PolicyWatcher::kUdpPortRangePolicyName,
                               kPortRange);
    port_range_empty_.SetString(PolicyWatcher::kUdpPortRangePolicyName,
                                std::string());

#if !defined(NDEBUG)
    SetDefaults(nat_false_overridden_others_default_);
    nat_false_overridden_others_default_.SetBoolean(
        PolicyWatcher::kNatPolicyName, false);
    nat_false_overridden_others_default_.SetString(
        PolicyWatcher::kHostDebugOverridePoliciesName,
        kOverrideNatTraversalToFalse);
#endif
  }

 protected:
  void StartWatching() {
    policy_watcher_->StartWatching(policy_callback_);
    base::RunLoop().RunUntilIdle();
  }

  void StopWatching() {
    base::WaitableEvent stop_event(false, false);
    policy_watcher_->StopWatching(&stop_event);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(true, stop_event.IsSignaled());
  }

  static const char* kHostDomain;
  static const char* kPortRange;
  base::MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockPolicyCallback mock_policy_callback_;
  PolicyWatcher::PolicyCallback policy_callback_;
  scoped_ptr<FakePolicyWatcher> policy_watcher_;
  base::DictionaryValue empty_;
  base::DictionaryValue nat_true_;
  base::DictionaryValue nat_false_;
  base::DictionaryValue nat_one_;
  base::DictionaryValue domain_empty_;
  base::DictionaryValue domain_full_;
  base::DictionaryValue nat_true_others_default_;
  base::DictionaryValue nat_false_others_default_;
  base::DictionaryValue domain_empty_others_default_;
  base::DictionaryValue domain_full_others_default_;
  base::DictionaryValue nat_true_domain_empty_;
  base::DictionaryValue nat_true_domain_full_;
  base::DictionaryValue nat_false_domain_empty_;
  base::DictionaryValue nat_false_domain_full_;
  base::DictionaryValue nat_true_domain_empty_others_default_;
  base::DictionaryValue unknown_policies_;
  base::DictionaryValue nat_true_and_overridden_;
  base::DictionaryValue nat_false_overridden_others_default_;
  base::DictionaryValue pairing_true_;
  base::DictionaryValue pairing_false_;
  base::DictionaryValue gnubby_auth_true_;
  base::DictionaryValue gnubby_auth_false_;
  base::DictionaryValue relay_true_;
  base::DictionaryValue relay_false_;
  base::DictionaryValue port_range_full_;
  base::DictionaryValue port_range_empty_;

 private:
  void SetDefaults(base::DictionaryValue& dict) {
    dict.SetBoolean(PolicyWatcher::kNatPolicyName, true);
    dict.SetBoolean(PolicyWatcher::kRelayPolicyName, true);
    dict.SetString(PolicyWatcher::kUdpPortRangePolicyName, "");
    dict.SetBoolean(PolicyWatcher::kHostRequireTwoFactorPolicyName, false);
    dict.SetString(PolicyWatcher::kHostDomainPolicyName, std::string());
    dict.SetBoolean(PolicyWatcher::kHostMatchUsernamePolicyName, false);
    dict.SetString(PolicyWatcher::kHostTalkGadgetPrefixPolicyName,
                   kDefaultHostTalkGadgetPrefix);
    dict.SetBoolean(PolicyWatcher::kHostRequireCurtainPolicyName, false);
    dict.SetString(PolicyWatcher::kHostTokenUrlPolicyName, std::string());
    dict.SetString(PolicyWatcher::kHostTokenValidationUrlPolicyName,
                   std::string());
    dict.SetString(PolicyWatcher::kHostTokenValidationCertIssuerPolicyName,
                   std::string());
    dict.SetBoolean(PolicyWatcher::kHostAllowClientPairing, true);
    dict.SetBoolean(PolicyWatcher::kHostAllowGnubbyAuthPolicyName, true);
#if !defined(NDEBUG)
    dict.SetString(PolicyWatcher::kHostDebugOverridePoliciesName, "");
#endif
  }
};

const char* PolicyWatcherTest::kHostDomain = "google.com";
const char* PolicyWatcherTest::kPortRange = "12400-12409";

MATCHER_P(IsPolicies, dict, "") {
  return arg->Equals(dict);
}

TEST_F(PolicyWatcherTest, None) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatFalse) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_false_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatOne) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_one_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, DomainEmpty) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&domain_empty_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, DomainFull) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&domain_full_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&nat_true_);
  policy_watcher_->SetPolicies(&nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrueThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&nat_true_);
  policy_watcher_->SetPolicies(&nat_true_);
  policy_watcher_->SetPolicies(&nat_false_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&nat_false_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenFalseThenTrue) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&nat_false_);
  policy_watcher_->SetPolicies(&nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, ChangeOneRepeatedlyThenTwo) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(
                  &nat_true_domain_empty_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_domain_full_)));

  StartWatching();
  policy_watcher_->SetPolicies(&nat_true_domain_empty_);
  policy_watcher_->SetPolicies(&nat_true_domain_full_);
  policy_watcher_->SetPolicies(&nat_false_domain_full_);
  policy_watcher_->SetPolicies(&nat_false_domain_empty_);
  policy_watcher_->SetPolicies(&nat_true_domain_full_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, FilterUnknownPolicies) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&unknown_policies_);
  policy_watcher_->SetPolicies(&empty_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, DebugOverrideNatPolicy) {
#if !defined(NDEBUG)
  EXPECT_CALL(mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&nat_false_overridden_others_default_)));
#else
  EXPECT_CALL(mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
#endif

  StartWatching();
  policy_watcher_->SetPolicies(&nat_true_and_overridden_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, PairingFalseThenTrue) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&pairing_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&pairing_true_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&pairing_false_);
  policy_watcher_->SetPolicies(&pairing_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, GnubbyAuth) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&gnubby_auth_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&gnubby_auth_true_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&gnubby_auth_false_);
  policy_watcher_->SetPolicies(&gnubby_auth_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, Relay) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&relay_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&relay_true_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&relay_false_);
  policy_watcher_->SetPolicies(&relay_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, UdpPortRange) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&port_range_full_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&port_range_empty_)));

  StartWatching();
  policy_watcher_->SetPolicies(&empty_);
  policy_watcher_->SetPolicies(&port_range_full_);
  policy_watcher_->SetPolicies(&port_range_empty_);
  StopWatching();
}

}  // namespace policy_hack
}  // namespace remoting
