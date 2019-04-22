// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mice_account_reconcilor_delegate.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_reconcilor.h"

namespace signin {

MiceAccountReconcilorDelegate::MiceAccountReconcilorDelegate() = default;

MiceAccountReconcilorDelegate::~MiceAccountReconcilorDelegate() = default;

bool MiceAccountReconcilorDelegate::IsReconcileEnabled() const {
  return true;
}

bool MiceAccountReconcilorDelegate::IsAccountConsistencyEnforced() const {
  return true;
}

gaia::GaiaSource MiceAccountReconcilorDelegate::GetGaiaApiSource() const {
  return gaia::GaiaSource::kAccountReconcilorMirror;
}

std::string MiceAccountReconcilorDelegate::GetFirstGaiaAccountForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string& primary_account,
    bool first_execution,
    bool will_logout) const {
  // This flow is deprecated and will be removed when multilogin is fully
  // launched.
  NOTREACHED() << "Mice requires multilogin";
  return std::string();
}

std::vector<std::string>
MiceAccountReconcilorDelegate::GetChromeAccountsForReconcile(
    const std::vector<std::string>& chrome_accounts,
    const std::string& primary_account,
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const gaia::MultiloginMode mode) const {
  if (chrome_accounts.empty())
    return {};

  // First account, by priority order:
  // - primary account
  // - first account on the device.
  // Warning: As a result, the reconciliation may change the default Gaia
  // account. It should be ensured that this is not surprising for the user.
  std::string new_first_account =
      base::ContainsValue(chrome_accounts, primary_account)
          ? primary_account
          : chrome_accounts[0];

  // Minimize account shuffling and ensure that the number of accounts does not
  // exceed the limit.
  return ReorderChromeAccountsForReconcile(chrome_accounts, new_first_account,
                                           gaia_accounts);
}

gaia::MultiloginMode MiceAccountReconcilorDelegate::CalculateModeForReconcile(
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::string primary_account,
    bool first_execution,
    bool primary_has_error) const {
  return gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
}

bool MiceAccountReconcilorDelegate::IsUnknownInvalidAccountInCookieAllowed()
    const {
  return false;
}

}  // namespace signin
