// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
#define CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeKerberosClient
    : public KerberosClient {
 public:
  FakeKerberosClient();
  ~FakeKerberosClient() override;

  // KerberosClient:
  void AddAccount(const kerberos::AddAccountRequest& request,
                  AddAccountCallback callback) override;
  void RemoveAccount(const kerberos::RemoveAccountRequest& request,
                     RemoveAccountCallback callback) override;
  void ClearAccounts(const kerberos::ClearAccountsRequest& request,
                     ClearAccountsCallback callback) override;
  void ListAccounts(const kerberos::ListAccountsRequest& request,
                    ListAccountsCallback callback) override;
  void SetConfig(const kerberos::SetConfigRequest& request,
                 SetConfigCallback callback) override;
  void ValidateConfig(const kerberos::ValidateConfigRequest& request,
                      ValidateConfigCallback callback) override;
  void AcquireKerberosTgt(const kerberos::AcquireKerberosTgtRequest& request,
                          int password_fd,
                          AcquireKerberosTgtCallback callback) override;
  void GetKerberosFiles(const kerberos::GetKerberosFilesRequest& request,
                        GetKerberosFilesCallback callback) override;
  void ConnectToKerberosFileChangedSignal(
      KerberosFilesChangedCallback callback) override;
  void ConnectToKerberosTicketExpiringSignal(
      KerberosTicketExpiringCallback callback) override;

 private:
  struct AccountData {
    // User principal (user@EXAMPLE.COM) that identifies this account.
    std::string principal_name;

    // Kerberos configuration file.
    std::string krb5conf;

    // True if AcquireKerberosTgt succeeded.
    bool has_tgt = false;

    // True if the account was added by policy.
    bool is_managed = false;

    // True if login password was used during last AcquireKerberosTgt() call.
    bool use_login_password = false;

    // Remembered password, if any.
    std::string password;

    explicit AccountData(const std::string& principal_name);
    AccountData(const AccountData& other);

    // Only compares principal_name. For finding and erasing in vectors.
    bool operator==(const AccountData& other) const;
    bool operator!=(const AccountData& other) const;
  };

  enum class WhatToRemove { kNothing, kPassword, kAccount };

  // Determines what data to remove, depending on |mode| and |data|.
  static WhatToRemove DetermineWhatToRemove(kerberos::ClearMode mode,
                                            const AccountData& data);

  // Returns the AccountData for |principal_name| if available or nullptr
  // otherwise.
  AccountData* GetAccountData(const std::string& principal_name);

  // Maps principal name (user@REALM.COM) to account data.
  std::vector<AccountData> accounts_;

  KerberosFilesChangedCallback kerberos_files_changed_callback_;
  KerberosTicketExpiringCallback kerberos_ticket_expiring_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeKerberosClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
