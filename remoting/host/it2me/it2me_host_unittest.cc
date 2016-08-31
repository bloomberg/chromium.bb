// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/policy_constants.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/policy_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

typedef protocol::ValidatingAuthenticator::Result ValidationResult;

const char kTestClientJid[] = "ficticious_user@gmail.com/jid_resource";
const char kTestClientUsernameNoJid[] = "completely_ficticious_user@gmail.com";
const char kTestClientJidWithSlash[] = "fake/user@gmail.com/jid_resource";
const char kMatchingDomain[] = "gmail.com";
const char kMismatchedDomain1[] = "similar_to_gmail.com";
const char kMismatchedDomain2[] = "gmail_at_the_beginning.com";
const char kMismatchedDomain3[] = "not_even_close.com";

}  // namespace

class It2MeHostTest : public testing::Test {
 public:
  It2MeHostTest() {}
  ~It2MeHostTest() override {}

  // testing::Test interface.
  void SetUp() override;
  void TearDown() override;

  void OnValidationComplete(const base::Closure& resume_callback,
                            ValidationResult validation_result);

 protected:
  void SetClientDomainPolicy(const std::string& policy_value);

  void RunValidationCallback(const std::string& remote_jid);

  ValidationResult validation_result_ = ValidationResult::SUCCESS;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;

  scoped_refptr<It2MeHost> it2me_host_;

  std::string directory_bot_jid_;
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHostTest);
};

void It2MeHostTest::SetUp() {
  message_loop_.reset(new base::MessageLoop());
  run_loop_.reset(new base::RunLoop());

  scoped_refptr<AutoThreadTaskRunner> auto_thread_task_runner =
      new AutoThreadTaskRunner(base::ThreadTaskRunnerHandle::Get(),
                               run_loop_->QuitClosure());
  it2me_host_ = new It2MeHost(
      ChromotingHostContext::Create(auto_thread_task_runner),
      /*policy_watcher=*/nullptr,
      /*confirmation_dialog_factory=*/nullptr,
      /*observer=*/nullptr, xmpp_server_config_, directory_bot_jid_);
}

void It2MeHostTest::TearDown() {
  it2me_host_ = nullptr;
  run_loop_->Run();
}

void It2MeHostTest::OnValidationComplete(const base::Closure& resume_callback,
                                         ValidationResult validation_result) {
  validation_result_ = validation_result;
  resume_callback.Run();
}

void It2MeHostTest::SetClientDomainPolicy(const std::string& policy_value) {
  std::unique_ptr<base::DictionaryValue> policies(new base::DictionaryValue());
  policies->SetString(policy::key::kRemoteAccessHostClientDomain, policy_value);

  base::RunLoop run_loop;
  it2me_host_->SetPolicyForTesting(std::move(policies), run_loop.QuitClosure());
  run_loop.Run();
}

void It2MeHostTest::RunValidationCallback(const std::string& remote_jid) {
  base::RunLoop run_loop;

  it2me_host_->GetValidationCallbackForTesting().Run(
      remote_jid, base::Bind(&It2MeHostTest::OnValidationComplete,
                             base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(It2MeHostTest, ConnectionValidation_NoClientDomainPolicy_ValidJid) {
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_NoClientDomainPolicy_InvalidJid) {
  RunValidationCallback(kTestClientUsernameNoJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
}

TEST_F(It2MeHostTest,
       ConnectionValidation_NoClientDomainPolicy_InvalidUsername) {
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainPolicy_MatchingDomain) {
  SetClientDomainPolicy(kMatchingDomain);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainPolicy_InvalidUserName) {
  SetClientDomainPolicy(kMatchingDomain);
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainPolicy_NoJid) {
  SetClientDomainPolicy(kMatchingDomain);
  RunValidationCallback(kTestClientUsernameNoJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_WrongClientDomain_NoMatch) {
  SetClientDomainPolicy(kMismatchedDomain3);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_WrongClientDomain_MatchStart) {
  SetClientDomainPolicy(kMismatchedDomain2);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_WrongClientDomain_MatchEnd) {
  SetClientDomainPolicy(kMismatchedDomain1);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

}  // namespace remoting
