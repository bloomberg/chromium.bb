// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/stub_authenticator_builder.h"

namespace chromeos {

StubAuthenticatorBuilder::StubAuthenticatorBuilder(
    const UserContext& expected_user_context)
    : expected_user_context_(expected_user_context) {}

StubAuthenticatorBuilder::~StubAuthenticatorBuilder() = default;

scoped_refptr<Authenticator> StubAuthenticatorBuilder::Create(
    AuthStatusConsumer* consumer) {
  scoped_refptr<StubAuthenticator> authenticator =
      new StubAuthenticator(consumer, expected_user_context_);
  authenticator->auth_action_ = auth_action_;
  if (auth_action_ == StubAuthenticator::AuthAction::kPasswordChange)
    authenticator->old_password_ = old_password_;
  if (data_recovery_notifier_)
    authenticator->data_recovery_notifier_ = data_recovery_notifier_;
  return authenticator;
}

void StubAuthenticatorBuilder::SetUpPasswordChange(
    const std::string& old_password,
    const StubAuthenticator::DataRecoveryNotifier& notifier) {
  auth_action_ = StubAuthenticator::AuthAction::kPasswordChange;
  old_password_ = old_password;
  data_recovery_notifier_ = notifier;
}

}  // namespace chromeos
