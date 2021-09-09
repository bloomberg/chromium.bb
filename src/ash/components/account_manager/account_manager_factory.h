// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_
#define ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ash.h"
#include "base/component_export.h"
#include "base/sequence_checker.h"

namespace ash {

// This factory is needed because of multi signin on Chrome OS. Device Accounts,
// which are simultaneously logged into Chrome OS, should see different
// instances of |AccountManager| and hence |AccountManager| cannot be a part of
// a global like |g_browser_process| (otherwise Device Accounts will start
// sharing |AccountManager| and by extension, their Secondary
// Accounts/Identities, which is undesirable).
// Once multi signin has been removed and multi profile on ChromeOS takes its
// place, remove this class and make |AccountManager| a part of
// |g_browser_process|.
class COMPONENT_EXPORT(ASH_COMPONENTS_ACCOUNT_MANAGER) AccountManagerFactory {
 public:
  AccountManagerFactory();
  AccountManagerFactory(const AccountManagerFactory&) = delete;
  AccountManagerFactory& operator=(const AccountManagerFactory&) = delete;
  ~AccountManagerFactory();

  // Returns the |AccountManager| corresponding to the given |profile_path|.
  AccountManager* GetAccountManager(const std::string& profile_path);

  // Returns the |AccountManagerAsh| corresponding to the given |profile_path|.
  crosapi::AccountManagerAsh* GetAccountManagerAsh(
      const std::string& profile_path);

 private:
  struct AccountManagerHolder {
    AccountManagerHolder(
        std::unique_ptr<AccountManager> account_manager,
        std::unique_ptr<crosapi::AccountManagerAsh> account_manager_ash);
    AccountManagerHolder(const AccountManagerHolder&) = delete;
    AccountManagerHolder& operator=(const AccountManagerHolder&) = delete;
    ~AccountManagerHolder();

    const std::unique_ptr<AccountManager> account_manager;
    const std::unique_ptr<crosapi::AccountManagerAsh> account_manager_ash;
  };

  const AccountManagerHolder& GetAccountManagerHolder(
      const std::string& profile_path);

  // A mapping from Profile path to an |AccountManagerHolder|. Acts a cache of
  // Account Managers and AccountManagerAsh objects.
  std::unordered_map<std::string, AccountManagerHolder> account_managers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // ASH_COMPONENTS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_FACTORY_H_
