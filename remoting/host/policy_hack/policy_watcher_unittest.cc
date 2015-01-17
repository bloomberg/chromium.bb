// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "policy/policy_constants.h"
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
  PolicyWatcherTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

  void SetUp() override {
    message_loop_proxy_ = base::MessageLoopProxy::current();
    policy_updated_callback_ = base::Bind(
        &MockPolicyCallback::OnPolicyUpdate,
        base::Unretained(&mock_policy_callback_));
    policy_error_callback_ = base::Bind(
        &MockPolicyCallback::OnPolicyError,
        base::Unretained(&mock_policy_callback_));
    policy_watcher_.reset(new FakePolicyWatcher(message_loop_proxy_));
    nat_true_.SetBoolean(policy::key::kRemoteAccessHostFirewallTraversal, true);
    nat_false_.SetBoolean(policy::key::kRemoteAccessHostFirewallTraversal,
                          false);
    nat_one_.SetInteger(policy::key::kRemoteAccessHostFirewallTraversal, 1);
    domain_empty_.SetString(policy::key::kRemoteAccessHostDomain,
                            std::string());
    domain_full_.SetString(policy::key::kRemoteAccessHostDomain, kHostDomain);
    SetDefaults(nat_true_others_default_);
    nat_true_others_default_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, true);
    SetDefaults(nat_false_others_default_);
    nat_false_others_default_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, false);
    SetDefaults(domain_empty_others_default_);
    domain_empty_others_default_.SetString(policy::key::kRemoteAccessHostDomain,
                                           std::string());
    SetDefaults(domain_full_others_default_);
    domain_full_others_default_.SetString(policy::key::kRemoteAccessHostDomain,
                                          kHostDomain);
    nat_true_domain_empty_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_empty_.SetString(policy::key::kRemoteAccessHostDomain,
                                     std::string());
    nat_true_domain_full_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_full_.SetString(policy::key::kRemoteAccessHostDomain,
                                    kHostDomain);
    nat_false_domain_empty_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, false);
    nat_false_domain_empty_.SetString(policy::key::kRemoteAccessHostDomain,
                                      std::string());
    nat_false_domain_full_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, false);
    nat_false_domain_full_.SetString(policy::key::kRemoteAccessHostDomain,
                                     kHostDomain);
    SetDefaults(nat_true_domain_empty_others_default_);
    nat_true_domain_empty_others_default_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_empty_others_default_.SetString(
        policy::key::kRemoteAccessHostDomain, std::string());
    unknown_policies_.SetString("UnknownPolicyOne", std::string());
    unknown_policies_.SetString("UnknownPolicyTwo", std::string());

    const char kOverrideNatTraversalToFalse[] =
      "{ \"RemoteAccessHostFirewallTraversal\": false }";
    nat_true_and_overridden_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_and_overridden_.SetString(
        policy::key::kRemoteAccessHostDebugOverridePolicies,
        kOverrideNatTraversalToFalse);
    pairing_true_.SetBoolean(policy::key::kRemoteAccessHostAllowClientPairing,
                             true);
    pairing_false_.SetBoolean(policy::key::kRemoteAccessHostAllowClientPairing,
                              false);
    gnubby_auth_true_.SetBoolean(policy::key::kRemoteAccessHostAllowGnubbyAuth,
                                 true);
    gnubby_auth_false_.SetBoolean(policy::key::kRemoteAccessHostAllowGnubbyAuth,
                                  false);
    relay_true_.SetBoolean(policy::key::kRemoteAccessHostAllowRelayedConnection,
                           true);
    relay_false_.SetBoolean(
        policy::key::kRemoteAccessHostAllowRelayedConnection, false);
    port_range_full_.SetString(policy::key::kRemoteAccessHostUdpPortRange,
                               kPortRange);
    port_range_empty_.SetString(policy::key::kRemoteAccessHostUdpPortRange,
                                std::string());

#if !defined(NDEBUG)
    SetDefaults(nat_false_overridden_others_default_);
    nat_false_overridden_others_default_.SetBoolean(
        policy::key::kRemoteAccessHostFirewallTraversal, false);
    nat_false_overridden_others_default_.SetString(
        policy::key::kRemoteAccessHostDebugOverridePolicies,
        kOverrideNatTraversalToFalse);
#endif
  }

 protected:
  void StartWatching() {
    policy_watcher_->StartWatching(
        policy_updated_callback_,
        policy_error_callback_);
    base::RunLoop().RunUntilIdle();
  }

  void StopWatching() {
    EXPECT_CALL(*this, PostPolicyWatcherShutdown()).Times(1);
    policy_watcher_->StopWatching(base::Bind(
        &PolicyWatcherTest::PostPolicyWatcherShutdown, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD0(PostPolicyWatcherShutdown, void());

  static const char* kHostDomain;
  static const char* kPortRange;
  base::MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockPolicyCallback mock_policy_callback_;
  PolicyWatcher::PolicyUpdatedCallback policy_updated_callback_;
  PolicyWatcher::PolicyErrorCallback policy_error_callback_;
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
    dict.SetBoolean(policy::key::kRemoteAccessHostFirewallTraversal, true);
    dict.SetBoolean(policy::key::kRemoteAccessHostAllowRelayedConnection, true);
    dict.SetString(policy::key::kRemoteAccessHostUdpPortRange, "");
    dict.SetBoolean(policy::key::kRemoteAccessHostRequireTwoFactor, false);
    dict.SetString(policy::key::kRemoteAccessHostDomain, std::string());
    dict.SetBoolean(policy::key::kRemoteAccessHostMatchUsername, false);
    dict.SetString(policy::key::kRemoteAccessHostTalkGadgetPrefix,
                   kDefaultHostTalkGadgetPrefix);
    dict.SetBoolean(policy::key::kRemoteAccessHostRequireCurtain, false);
    dict.SetString(policy::key::kRemoteAccessHostTokenUrl, std::string());
    dict.SetString(policy::key::kRemoteAccessHostTokenValidationUrl,
                   std::string());
    dict.SetString(
        policy::key::kRemoteAccessHostTokenValidationCertificateIssuer,
        std::string());
    dict.SetBoolean(policy::key::kRemoteAccessHostAllowClientPairing, true);
    dict.SetBoolean(policy::key::kRemoteAccessHostAllowGnubbyAuth, true);
#if !defined(NDEBUG)
    dict.SetString(policy::key::kRemoteAccessHostDebugOverridePolicies, "");
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

const int kMaxTransientErrorRetries = 5;

TEST_F(PolicyWatcherTest, SingleTransientErrorDoesntTriggerErrorCallback) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyErrorPtr()).Times(0);

  StartWatching();
  policy_watcher_->SignalTransientErrorForTest();
  StopWatching();
}

TEST_F(PolicyWatcherTest, MultipleTransientErrorsTriggerErrorCallback) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyErrorPtr());

  StartWatching();
  for (int i = 0; i < kMaxTransientErrorRetries; i++) {
    policy_watcher_->SignalTransientErrorForTest();
  }
  StopWatching();
}

TEST_F(PolicyWatcherTest, PolicyUpdateResetsTransientErrorsCounter) {
  testing::InSequence s;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyErrorPtr()).Times(0);

  StartWatching();
  for (int i = 0; i < (kMaxTransientErrorRetries - 1); i++) {
    policy_watcher_->SignalTransientErrorForTest();
  }
  policy_watcher_->SetPolicies(&nat_true_);
  for (int i = 0; i < (kMaxTransientErrorRetries - 1); i++) {
    policy_watcher_->SignalTransientErrorForTest();
  }
  StopWatching();
}

// Unit tests cannot instantiate PolicyWatcher on ChromeOS
// (as this requires running inside a browser process).
#ifndef OS_CHROMEOS

namespace {

void OnPolicyUpdatedDumpPolicy(scoped_ptr<base::DictionaryValue> policies) {
  VLOG(1) << "OnPolicyUpdated callback received the following policies:";

  for (base::DictionaryValue::Iterator iter(*policies); !iter.IsAtEnd();
       iter.Advance()) {
    switch (iter.value().GetType()) {
      case base::Value::Type::TYPE_STRING: {
        std::string value;
        CHECK(iter.value().GetAsString(&value));
        VLOG(1) << iter.key() << " = "
                << "string: " << '"' << value << '"';
        break;
      }
      case base::Value::Type::TYPE_BOOLEAN: {
        bool value;
        CHECK(iter.value().GetAsBoolean(&value));
        VLOG(1) << iter.key() << " = "
                << "boolean: " << (value ? "True" : "False");
        break;
      }
      default: {
        VLOG(1) << iter.key() << " = "
                << "unrecognized type";
        break;
      }
    }
  }
}

}  // anonymous namespace

// To dump policy contents, run unit tests with the following flags:
// out/Debug/remoting_unittests --gtest_filter=*TestRealChromotingPolicy* -v=1
TEST_F(PolicyWatcherTest, TestRealChromotingPolicy) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::MessageLoop::current()->task_runner();
  scoped_ptr<PolicyWatcher> policy_watcher(
      PolicyWatcher::Create(nullptr, task_runner));

  {
    base::RunLoop run_loop;
    policy_watcher->StartWatching(base::Bind(OnPolicyUpdatedDumpPolicy),
                                  base::Bind(base::DoNothing));
    run_loop.RunUntilIdle();
  }

  {
    base::RunLoop run_loop;
    PolicyWatcher* raw_policy_watcher = policy_watcher.release();
    raw_policy_watcher->StopWatching(
        base::Bind(base::IgnoreResult(
                       &base::SequencedTaskRunner::DeleteSoon<PolicyWatcher>),
                   task_runner, FROM_HERE, raw_policy_watcher));
    run_loop.RunUntilIdle();
  }

  // Today, the only verification offered by this test is:
  // - Manual verification of policy values dumped by OnPolicyUpdatedDumpPolicy
  // - Automated verification that nothing crashed
}

#endif

// TODO(lukasza): We should consider adding a test against a
// MockConfigurationPolicyProvider.

}  // namespace policy_hack
}  // namespace remoting
