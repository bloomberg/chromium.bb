// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BLACKLISTED_DUPLICATES_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BLACKLISTED_DUPLICATES_CLEANER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

class PrefService;

namespace password_manager {

class PasswordStore;

// This class is responsible for deleting blacklisted duplicates. Two
// blacklisted forms are considered equal if they have the same signon_realm.
// TODO(https://crbug.com/866794): Remove the code once majority of the users
// executed it.
class BlacklistedDuplicatesCleaner : public PasswordStoreConsumer,
                                     public CredentialsCleaner {
 public:
  BlacklistedDuplicatesCleaner(scoped_refptr<PasswordStore> store,
                               PrefService* prefs);
  ~BlacklistedDuplicatesCleaner() override;

  // CredentialsCleaner:
  void StartCleaning(Observer* observer) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  // Clean-up is performed on |store_|.
  scoped_refptr<PasswordStore> store_;

  // |prefs_| is used to record that the clean-up happened and thus does not
  // have to happen again. This is not an owning pointer, being owned by
  // ProfileImpl class.
  PrefService* prefs_;

  // Used to signal completion of the clean-up task. It is null until
  // StartCleaning is called.
  Observer* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BlacklistedDuplicatesCleaner);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BLACKLISTED_DUPLICATES_CLEANER_H_