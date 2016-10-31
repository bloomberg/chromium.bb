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
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/policy_constants.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Shortening some type names for readability.
typedef protocol::ValidatingAuthenticator::Result ValidationResult;
typedef It2MeConfirmationDialog::Result DialogResult;

const char kTestClientUserName[] = "ficticious_user@gmail.com";
const char kTestClientJid[] = "ficticious_user@gmail.com/jid_resource";
const char kTestClientUsernameNoJid[] = "completely_ficticious_user@gmail.com";
const char kTestClientJidWithSlash[] = "fake/user@gmail.com/jid_resource";
const char kResourceOnly[] = "/jid_resource";
const char kMatchingDomain[] = "gmail.com";
const char kMismatchedDomain1[] = "similar_to_gmail.com";
const char kMismatchedDomain2[] = "gmail_at_the_beginning.com";
const char kMismatchedDomain3[] = "not_even_close.com";

}  // namespace

class FakeIt2MeConfirmationDialog : public It2MeConfirmationDialog {
 public:
  FakeIt2MeConfirmationDialog();
  ~FakeIt2MeConfirmationDialog() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            const ResultCallback& callback) override;

  void set_dialog_result(DialogResult dialog_result) {
    dialog_result_ = dialog_result;
  }

  const std::string& get_remote_user_email() { return remote_user_email_; }

 private:
  std::string remote_user_email_;
  DialogResult dialog_result_ = DialogResult::OK;

  DISALLOW_COPY_AND_ASSIGN(FakeIt2MeConfirmationDialog);
};

FakeIt2MeConfirmationDialog::FakeIt2MeConfirmationDialog() {}

FakeIt2MeConfirmationDialog::~FakeIt2MeConfirmationDialog() {}

void FakeIt2MeConfirmationDialog::Show(const std::string& remote_user_email,
                                       const ResultCallback& callback) {
  remote_user_email_ = remote_user_email;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, dialog_result_));
}

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
  std::string remote_user_email_;

  // Used to set ConfirmationDialog behavior.
  FakeIt2MeConfirmationDialog* dialog_ = nullptr;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;

  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  scoped_refptr<It2MeHost> it2me_host_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHostTest);
};

void It2MeHostTest::SetUp() {
  message_loop_.reset(new base::MessageLoop());
  run_loop_.reset(new base::RunLoop());

  std::unique_ptr<ChromotingHostContext> host_context(
      ChromotingHostContext::Create(new AutoThreadTaskRunner(
          base::ThreadTaskRunnerHandle::Get(), run_loop_->QuitClosure())));
  network_task_runner_ = host_context->network_task_runner();
  ui_task_runner_ = host_context->ui_task_runner();

  dialog_ = new FakeIt2MeConfirmationDialog();
  it2me_host_ =
      new It2MeHost(std::move(host_context),
                    /*policy_watcher=*/nullptr, base::WrapUnique(dialog_),
                    /*observer=*/nullptr,
                    base::WrapUnique(new FakeSignalStrategy("fake_local_jid")),
                    "fake_user_name", "fake_bot_jid");
}

void It2MeHostTest::TearDown() {
  network_task_runner_ = nullptr;
  ui_task_runner_ = nullptr;
  it2me_host_ = nullptr;
  run_loop_->Run();
}

void It2MeHostTest::OnValidationComplete(const base::Closure& resume_callback,
                                         ValidationResult validation_result) {
  validation_result_ = validation_result;
  remote_user_email_ = dialog_->get_remote_user_email();

  ui_task_runner_->PostTask(FROM_HERE, resume_callback);
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

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::SetStateForTesting, it2me_host_.get(),
                            It2MeHostState::kStarting, std::string()));

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&It2MeHost::SetStateForTesting, it2me_host_.get(),
                 It2MeHostState::kRequestedAccessCode, std::string()));

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&It2MeHost::SetStateForTesting, it2me_host_.get(),
                 It2MeHostState::kReceivedAccessCode, std::string()));

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(it2me_host_->GetValidationCallbackForTesting(), remote_jid,
                 base::Bind(&It2MeHostTest::OnValidationComplete,
                            base::Unretained(this), run_loop.QuitClosure())));

  run_loop.Run();

  it2me_host_->Disconnect();
}

TEST_F(It2MeHostTest, ConnectionValidation_NoClientDomainPolicy_ValidJid) {
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_NoClientDomainPolicy_InvalidJid) {
  RunValidationCallback(kTestClientUsernameNoJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest,
       ConnectionValidation_NoClientDomainPolicy_InvalidUsername) {
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_NoClientDomainPolicy_ResourceOnly) {
  RunValidationCallback(kResourceOnly);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
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

TEST_F(It2MeHostTest, ConnectionValidation_ConfirmationDialog_Accept) {
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_STREQ(kTestClientUserName, remote_user_email_.c_str());
}

TEST_F(It2MeHostTest, ConnectionValidation_ConfirmationDialog_Reject) {
  dialog_->set_dialog_result(DialogResult::CANCEL);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_REJECTED_BY_USER, validation_result_);
  ASSERT_STREQ(kTestClientUserName, remote_user_email_.c_str());
}

}  // namespace remoting
