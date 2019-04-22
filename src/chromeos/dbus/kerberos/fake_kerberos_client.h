// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
#define CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/optional.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeKerberosClient
    : public KerberosClient,
      public KerberosClient::TestInterface {
 public:
  FakeKerberosClient();
  ~FakeKerberosClient() override;

  // KerberosClient:
  void AddAccount(const kerberos::AddAccountRequest& request,
                  AddAccountCallback callback) override;
  void RemoveAccount(const kerberos::RemoveAccountRequest& request,
                     RemoveAccountCallback callback) override;
  void SetConfig(const kerberos::SetConfigRequest& request,
                 SetConfigCallback callback) override;
  void AcquireKerberosTgt(const kerberos::AcquireKerberosTgtRequest& request,
                          int password_fd,
                          AcquireKerberosTgtCallback callback) override;
  void GetKerberosFiles(const kerberos::GetKerberosFilesRequest& request,
                        GetKerberosFilesCallback callback) override;
  void ConnectToKerberosFileChangedSignal(
      KerberosFilesChangedCallback callback) override;
  KerberosClient::TestInterface* GetTestInterface() override;

  // KerberosClient::TestInterface:
  void set_started(bool started) override;
  bool started() const override;

 private:
  struct AccountData {
    // Kerberos configuration file.
    std::string krb5conf;
    // Gets set to true if AcquireKerberosTgt succeeds.
    bool has_tgt = false;
  };

  // Returns the AccountData for |principal_name| if available or nullopt
  // otherwise.
  base::Optional<AccountData> GetAccountData(const std::string& principal_name);

  // Maps principal name (user@REALM.COM) to account data.
  using AccountsMap = std::unordered_map<std::string, AccountData>;
  AccountsMap accounts_;

  // Whether the service has started by UpstartClient.
  bool started_ = false;

  KerberosFilesChangedCallback kerberos_files_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeKerberosClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
