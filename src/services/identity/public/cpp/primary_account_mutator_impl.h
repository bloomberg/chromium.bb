// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_

#include "services/identity/public/cpp/primary_account_mutator.h"

class AccountTrackerService;
class SigninManager;

namespace identity {

// Concrete implementation of PrimaryAccountMutator that is based on the
// SigninManager API. It is supported on all platform except Chrome OS.
class PrimaryAccountMutatorImpl : public PrimaryAccountMutator {
 public:
  PrimaryAccountMutatorImpl(AccountTrackerService* account_tracker,
                            SigninManager* signin_manager);
  ~PrimaryAccountMutatorImpl() override;

  // PrimaryAccountMutator implementation.
  bool SetPrimaryAccount(const std::string& account_id) override;
  bool ClearPrimaryAccount(
      ClearAccountsAction action,
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) override;
  bool IsSettingPrimaryAccountAllowed() const override;
  void SetSettingPrimaryAccountAllowed(bool allowed) override;
  void SetAllowedPrimaryAccountPattern(const std::string& pattern) override;

 private:
  // Pointers to the services used by the PrimaryAccountMutatorImpl. They
  // *must* outlive this instance.
  AccountTrackerService* account_tracker_ = nullptr;
  SigninManager* signin_manager_ = nullptr;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_IMPL_H_
