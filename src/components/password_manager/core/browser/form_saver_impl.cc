// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;

namespace password_manager {

namespace {

// Remove all information from |form| that is not required for signature
// calculation.
void SanitizeFormData(FormData* form) {
  form->main_frame_origin = url::Origin();
  for (FormFieldData& field : form->fields) {
    field.label.clear();
    field.value.clear();
    field.autocomplete_attribute.clear();
    field.option_values.clear();
    field.option_contents.clear();
    field.placeholder.clear();
    field.css_classes.clear();
    field.id_attribute.clear();
    field.name_attribute.clear();
  }
}

}  // namespace

FormSaverImpl::FormSaverImpl(PasswordStore* store) : store_(store) {
  DCHECK(store);
}

FormSaverImpl::~FormSaverImpl() = default;

void FormSaverImpl::PermanentlyBlacklist(PasswordForm* observed) {
  observed->preferred = false;
  observed->blacklisted_by_user = true;
  observed->username_value.clear();
  observed->username_element.clear();
  observed->password_value.clear();
  observed->password_element.clear();
  observed->other_possible_usernames.clear();
  observed->date_created = base::Time::Now();
  observed->origin = observed->origin.GetOrigin();

  store_->AddLogin(*observed);
}

void FormSaverImpl::Save(const PasswordForm& pending,
                         const std::vector<const PasswordForm*>& matches,
                         const base::string16& old_password) {
  DCHECK(pending.preferred);
  DCHECK(!pending.blacklisted_by_user);
  // TODO(crbug/936011): move all the generation stuff out of here.
  if (presaved_)
    return SaveImpl(pending, false, {}, nullptr, nullptr);

  PasswordForm sanitized_pending(pending);
  SanitizeFormData(&sanitized_pending.form_data);
  store_->AddLogin(sanitized_pending);
  // Update existing matches in the password store.
  for (const auto* match : matches) {
    if (match->IsFederatedCredential())
      continue;
    // Delete obsolete empty username credentials.
    const bool same_password = match->password_value == pending.password_value;
    const bool username_was_added =
        match->username_value.empty() && !pending.username_value.empty();
    if (same_password && username_was_added && !match->is_public_suffix_match) {
      store_->RemoveLogin(*match);
      continue;
    }
    base::Optional<PasswordForm> form_to_update;
    const bool same_username = match->username_value == pending.username_value;
    if (same_username) {
      // Maybe update the password value.
      const bool form_has_old_password = match->password_value == old_password;
      if (form_has_old_password) {
        form_to_update = *match;
        form_to_update->password_value = pending.password_value;
      }
    } else if (match->preferred && !match->is_public_suffix_match) {
      // No other credential on the same security origin can be preferred but
      // the most recent one.
      form_to_update = *match;
      form_to_update->preferred = false;
    }
    if (form_to_update) {
      SanitizeFormData(&form_to_update->form_data);
      store_->UpdateLogin(std::move(*form_to_update));
    }
  }
}

void FormSaverImpl::Update(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  SaveImpl(pending, false, best_matches, credentials_to_update,
           old_primary_key);
}

void FormSaverImpl::PresaveGeneratedPassword(const PasswordForm& generated) {
  DCHECK_NE(base::string16(), generated.password_value);
  auto form = std::make_unique<PasswordForm>(generated);
  SanitizeFormData(&form->form_data);
  if (presaved_)
    store_->UpdateLoginWithPrimaryKey(*form, *presaved_);
  else
    store_->AddLogin(*form);
  presaved_ = std::move(form);
}

void FormSaverImpl::RemovePresavedPassword() {
  if (!presaved_)
    return;

  store_->RemoveLogin(*presaved_);
  presaved_ = nullptr;
}

void FormSaverImpl::Remove(const PasswordForm& form) {
  store_->RemoveLogin(form);
}

std::unique_ptr<FormSaver> FormSaverImpl::Clone() {
  auto result = std::make_unique<FormSaverImpl>(store_);
  if (presaved_)
    result->presaved_ = std::make_unique<PasswordForm>(*presaved_);
  return result;
}

void FormSaverImpl::SaveImpl(
    const PasswordForm& pending,
    bool is_new_login,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  DCHECK(pending.preferred);
  DCHECK(!pending.blacklisted_by_user);

  UpdatePreferredLoginState(pending.username_value, best_matches);
  PasswordForm sanitized_pending(pending);
  SanitizeFormData(&sanitized_pending.form_data);
  if (presaved_) {
    store_->UpdateLoginWithPrimaryKey(sanitized_pending, *presaved_);
    presaved_ = nullptr;
  } else if (is_new_login) {
    store_->AddLogin(sanitized_pending);
    if (!sanitized_pending.username_value.empty())
      DeleteEmptyUsernameCredentials(sanitized_pending, best_matches);
  } else {
    if (old_primary_key)
      store_->UpdateLoginWithPrimaryKey(sanitized_pending, *old_primary_key);
    else
      store_->UpdateLogin(sanitized_pending);
  }

  if (credentials_to_update) {
    for (const PasswordForm& credential : *credentials_to_update) {
      store_->UpdateLogin(credential);
    }
  }
}

void FormSaverImpl::UpdatePreferredLoginState(
    const base::string16& preferred_username,
    const std::map<base::string16, const PasswordForm*>& best_matches) {
  for (const auto& key_value_pair : best_matches) {
    const PasswordForm& form = *key_value_pair.second;
    if (form.preferred && !form.is_public_suffix_match &&
        form.username_value != preferred_username) {
      // This wasn't the selected login but it used to be preferred.
      PasswordForm update(form);
      SanitizeFormData(&update.form_data);
      update.preferred = false;
      store_->UpdateLogin(update);
    }
  }
}

void FormSaverImpl::DeleteEmptyUsernameCredentials(
    const PasswordForm& pending,
    const std::map<base::string16, const PasswordForm*>& best_matches) {
  DCHECK(!pending.username_value.empty());

  for (const auto& match : best_matches) {
    const PasswordForm* form = match.second;
    if (!form->is_public_suffix_match && form->username_value.empty() &&
        form->password_value == pending.password_value) {
      store_->RemoveLogin(*form);
    }
  }
}

}  // namespace password_manager
