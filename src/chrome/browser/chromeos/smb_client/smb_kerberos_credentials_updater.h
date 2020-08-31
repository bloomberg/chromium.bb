// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_KERBEROS_CREDENTIALS_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_KERBEROS_CREDENTIALS_UPDATER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"

namespace chromeos {

namespace smb_client {

// Updates Kerberos credentials in SmbService after receiving
// OnAccountsChanged notification from KerberosCredentialsManager.
class SmbKerberosCredentialsUpdater
    : public KerberosCredentialsManager::Observer {
 public:
  using ActiveAccountChangedCallback =
      base::RepeatingCallback<void(const std::string& account_identifier)>;

  SmbKerberosCredentialsUpdater(
      KerberosCredentialsManager* credentials_manager,
      ActiveAccountChangedCallback active_account_changed_callback);

  ~SmbKerberosCredentialsUpdater() override;

  // Checks if Kerberos is enabled by asking KerberosCredentialsManager.
  bool IsKerberosEnabled() const;

  const std::string& active_account_name() const {
    return active_account_name_;
  }

 private:
  // KerberosCredentialsManager::Observer:
  void OnAccountsChanged() override;

  KerberosCredentialsManager* credentials_manager_;  // Not owned.
  std::string active_account_name_;
  const ActiveAccountChangedCallback active_account_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(SmbKerberosCredentialsUpdater);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_KERBEROS_CREDENTIALS_UPDATER_H_
