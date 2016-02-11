// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/mock_log.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "policy/policy_constants.h"
#include "remoting/host/dns_blackhole_checker.h"
#include "remoting/host/policy_watcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace key = ::policy::key;

MATCHER_P(IsPolicies, dict, "") {
  bool equal = arg->Equals(dict);
  if (!equal) {
    std::string actual_value;
    base::JSONWriter::WriteWithOptions(
        *arg, base::JSONWriter::OPTIONS_PRETTY_PRINT, &actual_value);

    std::string expected_value;
    base::JSONWriter::WriteWithOptions(
        *dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &expected_value);

    *result_listener << "Policies are not equal. ";
    *result_listener << "Expected policy: " << expected_value << ". ";
    *result_listener << "Actual policy: " << actual_value << ".";
  }
  return equal;
}

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
    // We expect no callbacks unless explicitly specified by individual tests.
    EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(testing::_)).Times(0);
    EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(0);

    // Retaining a raw pointer to keep control over policy contents.
    policy_loader_ =
        new policy::FakeAsyncPolicyLoader(base::ThreadTaskRunnerHandle::Get());
    policy_watcher_ =
        PolicyWatcher::CreateFromPolicyLoader(make_scoped_ptr(policy_loader_));

    nat_true_.SetBoolean(key::kRemoteAccessHostFirewallTraversal, true);
    nat_false_.SetBoolean(key::kRemoteAccessHostFirewallTraversal, false);
    nat_one_.SetInteger(key::kRemoteAccessHostFirewallTraversal, 1);
    nat_one_domain_full_.SetInteger(key::kRemoteAccessHostFirewallTraversal, 1);
    nat_one_domain_full_.SetString(key::kRemoteAccessHostDomain, kHostDomain);
    domain_empty_.SetString(key::kRemoteAccessHostDomain, std::string());
    domain_full_.SetString(key::kRemoteAccessHostDomain, kHostDomain);
    SetDefaults(nat_true_others_default_);
    nat_true_others_default_.SetBoolean(key::kRemoteAccessHostFirewallTraversal,
                                        true);
    SetDefaults(nat_false_others_default_);
    nat_false_others_default_.SetBoolean(
        key::kRemoteAccessHostFirewallTraversal, false);
    SetDefaults(domain_empty_others_default_);
    domain_empty_others_default_.SetString(key::kRemoteAccessHostDomain,
                                           std::string());
    SetDefaults(domain_full_others_default_);
    domain_full_others_default_.SetString(key::kRemoteAccessHostDomain,
                                          kHostDomain);
    nat_true_domain_empty_.SetBoolean(key::kRemoteAccessHostFirewallTraversal,
                                      true);
    nat_true_domain_empty_.SetString(key::kRemoteAccessHostDomain,
                                     std::string());
    nat_true_domain_full_.SetBoolean(key::kRemoteAccessHostFirewallTraversal,
                                     true);
    nat_true_domain_full_.SetString(key::kRemoteAccessHostDomain, kHostDomain);
    nat_false_domain_empty_.SetBoolean(key::kRemoteAccessHostFirewallTraversal,
                                       false);
    nat_false_domain_empty_.SetString(key::kRemoteAccessHostDomain,
                                      std::string());
    nat_false_domain_full_.SetBoolean(key::kRemoteAccessHostFirewallTraversal,
                                      false);
    nat_false_domain_full_.SetString(key::kRemoteAccessHostDomain, kHostDomain);
    SetDefaults(nat_true_domain_empty_others_default_);
    nat_true_domain_empty_others_default_.SetBoolean(
        key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_empty_others_default_.SetString(
        key::kRemoteAccessHostDomain, std::string());
    unknown_policies_.SetString("UnknownPolicyOne", std::string());
    unknown_policies_.SetString("UnknownPolicyTwo", std::string());
    unknown_policies_.SetBoolean("RemoteAccessHostUnknownPolicyThree", true);

    pairing_true_.SetBoolean(key::kRemoteAccessHostAllowClientPairing, true);
    pairing_false_.SetBoolean(key::kRemoteAccessHostAllowClientPairing, false);
    gnubby_auth_true_.SetBoolean(key::kRemoteAccessHostAllowGnubbyAuth, true);
    gnubby_auth_false_.SetBoolean(key::kRemoteAccessHostAllowGnubbyAuth, false);
    relay_true_.SetBoolean(key::kRemoteAccessHostAllowRelayedConnection, true);
    relay_false_.SetBoolean(key::kRemoteAccessHostAllowRelayedConnection,
                            false);
    port_range_full_.SetString(key::kRemoteAccessHostUdpPortRange, kPortRange);
    port_range_empty_.SetString(key::kRemoteAccessHostUdpPortRange,
                                std::string());
    port_range_malformed_.SetString(key::kRemoteAccessHostUdpPortRange,
                                    "malformed");
    port_range_malformed_domain_full_.MergeDictionary(&port_range_malformed_);
    port_range_malformed_domain_full_.SetString(key::kRemoteAccessHostDomain,
                                                kHostDomain);

    curtain_true_.SetBoolean(key::kRemoteAccessHostRequireCurtain, true);
    curtain_false_.SetBoolean(key::kRemoteAccessHostRequireCurtain, false);
    username_true_.SetBoolean(key::kRemoteAccessHostMatchUsername, true);
    username_false_.SetBoolean(key::kRemoteAccessHostMatchUsername, false);
    talk_gadget_blah_.SetString(key::kRemoteAccessHostTalkGadgetPrefix, "blah");
    third_party_auth_partial_.SetString(key::kRemoteAccessHostTokenUrl,
                                        "https://token.com");
    third_party_auth_partial_.SetString(
        key::kRemoteAccessHostTokenValidationUrl, "https://validation.com");
    third_party_auth_full_.MergeDictionary(&third_party_auth_partial_);
    third_party_auth_full_.SetString(
        key::kRemoteAccessHostTokenValidationCertificateIssuer,
        "certificate subject");
    third_party_auth_cert_empty_.MergeDictionary(&third_party_auth_partial_);
    third_party_auth_cert_empty_.SetString(
        key::kRemoteAccessHostTokenValidationCertificateIssuer, "");
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

  void SetPolicies(const base::DictionaryValue& dict) {
    // Copy |dict| into |policy_bundle|.
    policy::PolicyNamespace policy_namespace =
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
    policy::PolicyBundle policy_bundle;
    policy::PolicyMap& policy_map = policy_bundle.Get(policy_namespace);
    policy_map.LoadFrom(&dict, policy::POLICY_LEVEL_MANDATORY,
                        policy::POLICY_SCOPE_MACHINE,
                        policy::POLICY_SOURCE_CLOUD);

    // Simulate a policy file/registry/preference update.
    policy_loader_->SetPolicies(policy_bundle);
    policy_loader_->PostReloadOnBackgroundThread(true /* force reload asap */);
    base::RunLoop().RunUntilIdle();
  }

  const policy::Schema* GetPolicySchema() {
    return policy_watcher_->GetPolicySchema();
  }

  const base::DictionaryValue& GetDefaultValues() {
    return *(policy_watcher_->default_values_);
  }

  MOCK_METHOD0(PostPolicyWatcherShutdown, void());

  static const char* kHostDomain;
  static const char* kPortRange;
  base::MessageLoop message_loop_;
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
  base::DictionaryValue nat_one_domain_full_;
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
  base::DictionaryValue pairing_true_;
  base::DictionaryValue pairing_false_;
  base::DictionaryValue gnubby_auth_true_;
  base::DictionaryValue gnubby_auth_false_;
  base::DictionaryValue relay_true_;
  base::DictionaryValue relay_false_;
  base::DictionaryValue port_range_full_;
  base::DictionaryValue port_range_empty_;
  base::DictionaryValue port_range_malformed_;
  base::DictionaryValue port_range_malformed_domain_full_;
  base::DictionaryValue curtain_true_;
  base::DictionaryValue curtain_false_;
  base::DictionaryValue username_true_;
  base::DictionaryValue username_false_;
  base::DictionaryValue talk_gadget_blah_;
  base::DictionaryValue third_party_auth_full_;
  base::DictionaryValue third_party_auth_partial_;
  base::DictionaryValue third_party_auth_cert_empty_;

 private:
  void SetDefaults(base::DictionaryValue& dict) {
    dict.SetBoolean(key::kRemoteAccessHostFirewallTraversal, true);
    dict.SetBoolean(key::kRemoteAccessHostAllowRelayedConnection, true);
    dict.SetString(key::kRemoteAccessHostUdpPortRange, "");
    dict.SetString(key::kRemoteAccessHostClientDomain, std::string());
    dict.SetString(key::kRemoteAccessHostDomain, std::string());
    dict.SetBoolean(key::kRemoteAccessHostMatchUsername, false);
    dict.SetString(key::kRemoteAccessHostTalkGadgetPrefix,
                   kDefaultHostTalkGadgetPrefix);
    dict.SetBoolean(key::kRemoteAccessHostRequireCurtain, false);
    dict.SetString(key::kRemoteAccessHostTokenUrl, "");
    dict.SetString(key::kRemoteAccessHostTokenValidationUrl, "");
    dict.SetString(key::kRemoteAccessHostTokenValidationCertificateIssuer, "");
    dict.SetBoolean(key::kRemoteAccessHostAllowClientPairing, true);
    dict.SetBoolean(key::kRemoteAccessHostAllowGnubbyAuth, true);

    ASSERT_THAT(&dict, IsPolicies(&GetDefaultValues()))
        << "Sanity check that defaults expected by the test code "
        << "match what is stored in PolicyWatcher::default_values_";
  }
};

const char* PolicyWatcherTest::kHostDomain = "google.com";
const char* PolicyWatcherTest::kPortRange = "12400-12409";

TEST_F(PolicyWatcherTest, None) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(nat_true_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatFalse) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_others_default_)));

  SetPolicies(nat_false_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatWrongType) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError());

  SetPolicies(nat_one_);
  StartWatching();
}

// This test verifies that a mistyped policy value is still detected
// even though it doesn't change during the second SetPolicies call.
TEST_F(PolicyWatcherTest, NatWrongTypeThenIrrelevantChange) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(2);

  SetPolicies(nat_one_);
  StartWatching();
  SetPolicies(nat_one_domain_full_);
}

// This test verifies that a malformed policy value is still detected
// even though it doesn't change during the second SetPolicies call.
TEST_F(PolicyWatcherTest, PortRangeMalformedThenIrrelevantChange) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(2);

  SetPolicies(port_range_malformed_);
  StartWatching();
  SetPolicies(port_range_malformed_domain_full_);
}

TEST_F(PolicyWatcherTest, DomainEmpty) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_others_default_)));

  SetPolicies(domain_empty_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, DomainFull) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_others_default_)));

  SetPolicies(domain_full_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
  SetPolicies(nat_true_);
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
}

TEST_F(PolicyWatcherTest, FilterUnknownPolicies) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(unknown_policies_);
  SetPolicies(empty_);
}

class MisspelledPolicyTest : public PolicyWatcherTest,
                             public ::testing::WithParamInterface<const char*> {
};

// Verify that a misspelled policy causes a warning written to the log.
TEST_P(MisspelledPolicyTest, WarningLogged) {
  const char* misspelled_policy_name = GetParam();
  base::test::MockLog mock_log;

  ON_CALL(mock_log, Log(testing::_, testing::_, testing::_, testing::_,
                        testing::_)).WillByDefault(testing::Return(true));

  EXPECT_CALL(mock_log,
              Log(logging::LOG_WARNING, testing::_, testing::_, testing::_,
                  testing::HasSubstr(misspelled_policy_name))).Times(1);

  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  base::DictionaryValue misspelled_policies;
  misspelled_policies.SetString(misspelled_policy_name, "some test value");
  mock_log.StartCapturingLogs();

  SetPolicies(misspelled_policies);
  StartWatching();

  mock_log.StopCapturingLogs();
}

INSTANTIATE_TEST_CASE_P(
    PolicyWatcherTest,
    MisspelledPolicyTest,
    ::testing::Values("RemoteAccessHostDomainX",
                      "XRemoteAccessHostDomain",
                      "RemoteAccessHostdomain",
                      "RemoteAccessHostPolicyForFutureVersion"));

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
}

TEST_F(PolicyWatcherTest, Curtain) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&curtain_true_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&curtain_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(curtain_true_);
  SetPolicies(curtain_false_);
}

TEST_F(PolicyWatcherTest, MatchUsername) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
#if !defined(OS_WIN)
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&username_true_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&username_false_)));
#else
// On Windows the MatchUsername policy is ignored and therefore the 2
// SetPolicies calls won't result in any calls to OnPolicyUpdate.
#endif

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(username_true_);
  SetPolicies(username_false_);
}

TEST_F(PolicyWatcherTest, TalkGadgetPrefix) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&talk_gadget_blah_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(talk_gadget_blah_);
}

TEST_F(PolicyWatcherTest, ThirdPartyAuthFull) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&third_party_auth_full_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(third_party_auth_full_);
}

// This test verifies what happens when only 1 out of 3 third-party auth
// policies changes.  Without the other 2 policy values such policy values
// combination is invalid (i.e. cannot have TokenUrl without
// TokenValidationUrl) and can trigger OnPolicyError unless PolicyWatcher
// implementation is careful around this scenario.
TEST_F(PolicyWatcherTest, ThirdPartyAuthPartialToFull) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&third_party_auth_cert_empty_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&third_party_auth_full_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(third_party_auth_partial_);
  SetPolicies(third_party_auth_full_);
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
}

TEST_F(PolicyWatcherTest, PolicySchemaAndPolicyWatcherShouldBeInSync) {
  // This test verifies that
  // 1) policy schema (generated out of policy_templates.json)
  // and
  // 2) PolicyWatcher's code (i.e. contents of the |default_values_| field)
  // are kept in-sync.

  std::map<std::string, base::Value::Type> expected_schema;
  for (base::DictionaryValue::Iterator i(GetDefaultValues()); !i.IsAtEnd();
       i.Advance()) {
    expected_schema[i.key()] = i.value().GetType();
  }
#if defined(OS_WIN)
  // RemoteAccessHostMatchUsername is marked in policy_templates.json as not
  // supported on Windows and therefore is (by design) excluded from the schema.
  expected_schema.erase(key::kRemoteAccessHostMatchUsername);
#endif

  std::map<std::string, base::Value::Type> actual_schema;
  const policy::Schema* schema = GetPolicySchema();
  ASSERT_TRUE(schema->valid());
  for (auto it = schema->GetPropertiesIterator(); !it.IsAtEnd(); it.Advance()) {
    std::string key = it.key();
    if (key.find("RemoteAccessHost") == std::string::npos) {
      // For now PolicyWatcher::GetPolicySchema() mixes Chrome and Chromoting
      // policies, so we have to skip them here.
      continue;
    }
    actual_schema[key] = it.schema().type();
  }

  EXPECT_THAT(actual_schema, testing::ContainerEq(expected_schema));
}

TEST_F(PolicyWatcherTest, SchemaTypeCheck) {
  const policy::Schema* schema = GetPolicySchema();
  ASSERT_TRUE(schema->valid());

  // Check one, random "string" policy to see if the type propagated correctly
  // from policy_templates.json file.
  const policy::Schema string_schema =
      schema->GetKnownProperty("RemoteAccessHostDomain");
  EXPECT_TRUE(string_schema.valid());
  EXPECT_EQ(string_schema.type(), base::Value::Type::TYPE_STRING);

  // And check one, random "boolean" policy to see if the type propagated
  // correctly from policy_templates.json file.
  const policy::Schema boolean_schema =
      schema->GetKnownProperty("RemoteAccessHostRequireCurtain");
  EXPECT_TRUE(boolean_schema.valid());
  EXPECT_EQ(boolean_schema.type(), base::Value::Type::TYPE_BOOLEAN);
}

}  // namespace remoting
