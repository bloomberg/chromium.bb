// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "policy/policy_constants.h"
#include "remoting/host/dns_blackhole_checker.h"
#include "remoting/host/policy_watcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class MockPolicyCallback {
 public:
  MockPolicyCallback(){};

  // TODO(lukasza): gmock cannot mock a method taking scoped_ptr<T>...
  MOCK_METHOD1(OnPolicyUpdatePtr, void(const base::DictionaryValue* policies));
  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies) {
    OnPolicyUpdatePtr(policies.get());
  }

  MOCK_METHOD0(OnPolicyError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicyCallback);
};

class PolicyWatcherTest : public testing::Test {
 public:
  PolicyWatcherTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

  void SetUp() override {
    message_loop_proxy_ = base::MessageLoopProxy::current();

    // Retaining a raw pointer to keep control over policy contents.
    policy_loader_ = new policy::FakeAsyncPolicyLoader(message_loop_proxy_);
    policy_watcher_ = PolicyWatcher::CreateFromPolicyLoader(
        message_loop_proxy_, make_scoped_ptr(policy_loader_));

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

  void TearDown() override {
    policy_watcher_.reset();
    policy_loader_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void StartWatching() {
    policy_watcher_->StartWatching(
        base::Bind(&MockPolicyCallback::OnPolicyUpdate,
                   base::Unretained(&mock_policy_callback_)),
        base::Bind(&MockPolicyCallback::OnPolicyError,
                   base::Unretained(&mock_policy_callback_)));
    base::RunLoop().RunUntilIdle();
  }

  void StopWatching() {
    EXPECT_CALL(*this, PostPolicyWatcherShutdown()).Times(1);
    policy_watcher_->StopWatching(base::Bind(
        &PolicyWatcherTest::PostPolicyWatcherShutdown, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void SetPolicies(const base::DictionaryValue& dict) {
    // Copy |dict| into |policy_bundle|.
    policy::PolicyNamespace policy_namespace =
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
    policy::PolicyBundle policy_bundle;
    policy::PolicyMap& policy_map = policy_bundle.Get(policy_namespace);
    policy_map.LoadFrom(&dict, policy::POLICY_LEVEL_MANDATORY,
                        policy::POLICY_SCOPE_MACHINE);

    // Simulate a policy file/registry/preference update.
    policy_loader_->SetPolicies(policy_bundle);
    policy_loader_->PostReloadOnBackgroundThread(true /* force reload asap */);
    base::RunLoop().RunUntilIdle();
  }

  void SignalTransientErrorForTest() {
    policy_watcher_->SignalTransientPolicyError();
  }

  MOCK_METHOD0(PostPolicyWatcherShutdown, void());

  static const char* kHostDomain;
  static const char* kPortRange;
  base::MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  MockPolicyCallback mock_policy_callback_;

  // |policy_loader_| is owned by |policy_watcher_|. PolicyWatcherTest retains
  // a raw pointer to |policy_loader_| in order to control the simulated / faked
  // policy contents.
  policy::FakeAsyncPolicyLoader* policy_loader_;
  scoped_ptr<PolicyWatcher> policy_watcher_;

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
  bool equal = arg->Equals(dict);
  if (!equal) {
    std::string actual_value;
    base::JSONWriter::WriteWithOptions(
        arg, base::JSONWriter::OPTIONS_PRETTY_PRINT, &actual_value);

    std::string expected_value;
    base::JSONWriter::WriteWithOptions(
        dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &expected_value);

    *result_listener << "Policies are not equal. ";
    *result_listener << "Expected policy: " << expected_value << ". ";
    *result_listener << "Actual policy: " << actual_value << ".";
  }
  return equal;
}

TEST_F(PolicyWatcherTest, None) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(nat_true_);
  StartWatching();
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatFalse) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_others_default_)));

  SetPolicies(nat_false_);
  StartWatching();
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatOne) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_others_default_)));

  SetPolicies(nat_one_);
  StartWatching();
  StopWatching();
}

TEST_F(PolicyWatcherTest, DomainEmpty) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_others_default_)));

  SetPolicies(domain_empty_);
  StartWatching();
  StopWatching();
}

TEST_F(PolicyWatcherTest, DomainFull) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_others_default_)));

  SetPolicies(domain_full_);
  StartWatching();
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
  SetPolicies(nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrueThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
  SetPolicies(nat_true_);
  SetPolicies(nat_false_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_false_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenFalseThenTrue) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_false_);
  SetPolicies(nat_true_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, ChangeOneRepeatedlyThenTwo) {
  testing::InSequence sequence;
  EXPECT_CALL(
      mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&nat_true_domain_empty_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_domain_full_)));

  SetPolicies(nat_true_domain_empty_);
  StartWatching();
  SetPolicies(nat_true_domain_full_);
  SetPolicies(nat_false_domain_full_);
  SetPolicies(nat_false_domain_empty_);
  SetPolicies(nat_true_domain_full_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, FilterUnknownPolicies) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(unknown_policies_);
  SetPolicies(empty_);
  StopWatching();
}

TEST_F(PolicyWatcherTest, DebugOverrideNatPolicy) {
#if !defined(NDEBUG)
  EXPECT_CALL(
      mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&nat_false_overridden_others_default_)));
#else
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
#endif

  SetPolicies(nat_true_and_overridden_);
  StartWatching();
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

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(pairing_false_);
  SetPolicies(pairing_true_);
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

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(gnubby_auth_false_);
  SetPolicies(gnubby_auth_true_);
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

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(relay_false_);
  SetPolicies(relay_true_);
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

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(port_range_full_);
  SetPolicies(port_range_empty_);
  StopWatching();
}

const int kMaxTransientErrorRetries = 5;

TEST_F(PolicyWatcherTest, SingleTransientErrorDoesntTriggerErrorCallback) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(0);

  StartWatching();
  SignalTransientErrorForTest();
  StopWatching();
}

TEST_F(PolicyWatcherTest, MultipleTransientErrorsTriggerErrorCallback) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError());

  StartWatching();
  for (int i = 0; i < kMaxTransientErrorRetries; i++) {
    SignalTransientErrorForTest();
  }
  StopWatching();
}

TEST_F(PolicyWatcherTest, PolicyUpdateResetsTransientErrorsCounter) {
  testing::InSequence s;
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(testing::_));
  EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(0);

  StartWatching();
  for (int i = 0; i < (kMaxTransientErrorRetries - 1); i++) {
    SignalTransientErrorForTest();
  }
  SetPolicies(nat_true_);
  for (int i = 0; i < (kMaxTransientErrorRetries - 1); i++) {
    SignalTransientErrorForTest();
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

}  // namespace remoting
