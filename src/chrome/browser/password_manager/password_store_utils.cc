// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_utils.h"

#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"

namespace {
using password_manager::metrics_util::IsPasswordChanged;
using password_manager::metrics_util::IsUsernameChanged;
}  // namespace

// TODO(crbug.com/1136815): Drop this, once Android no longer use it.
void EditSavedPasswords(
    Profile* profile,
    const base::span<const std::unique_ptr<password_manager::PasswordForm>>
        forms_to_change,
    const std::u16string& new_username,
    const absl::optional<std::u16string>& new_password) {
  DCHECK(!forms_to_change.empty());

  const std::string signon_realm = forms_to_change[0]->signon_realm;

  IsUsernameChanged username_changed(new_username !=
                                     forms_to_change[0]->username_value);
  IsPasswordChanged password_changed(new_password !=
                                     forms_to_change[0]->password_value);

  // An updated username implies a change in the primary key, thus we need to
  // make sure to call the right API. Update every entry in the equivalence
  // class.
  for (const auto& old_form : forms_to_change) {
    scoped_refptr<password_manager::PasswordStore> store =
        GetPasswordStore(profile, old_form->IsUsingAccountStore());

    if (!store) {
      continue;
    }
    password_manager::PasswordForm new_form = *old_form;
    new_form.username_value = new_username;

    // The desktop logic allows to edit usernames even in cases when the
    // password cannot be displayed. In those cases, new_password won't have
    // a value.
    if (new_password.has_value())
      new_form.password_value = new_password.value();
    if (username_changed) {
      store->UpdateLoginWithPrimaryKey(new_form, *old_form);
    } else {
      store->UpdateLogin(new_form);
    }
  }
  password_manager::metrics_util::LogPasswordEditResult(username_changed,
                                                        password_changed);
}

scoped_refptr<password_manager::PasswordStore> GetPasswordStore(
    Profile* profile,
    bool use_account_store) {
  if (use_account_store) {
    return AccountPasswordStoreFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
  }
  return PasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}
