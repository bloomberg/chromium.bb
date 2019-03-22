// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

// This interface allows the caller to save passwords and blacklist entries in
// a password store.
class FormSaver {
 public:
  FormSaver() = default;

  virtual ~FormSaver() = default;

  // Configures the |observed| form to become a blacklist entry and saves it.
  virtual void PermanentlyBlacklist(autofill::PasswordForm* observed) = 0;

  // Saves the |pending| form and updates the stored preference on
  // |best_matches|.
  virtual void Save(
      const autofill::PasswordForm& pending,
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches) = 0;

  // Updates the |pending| form and updates the stored preference on
  // |best_matches|. If |old_primary_key| is given, uses it for saving
  // |pending|. It also updates the password store with all
  // |credentials_to_update|.
  virtual void Update(
      const autofill::PasswordForm& pending,
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const std::vector<autofill::PasswordForm>* credentials_to_update,
      const autofill::PasswordForm* old_primary_key) = 0;

  // Ensures that |generated| is saved in the store. This is in ideal case
  // either followed by a call to Save or RemovePresavedPassword. But if the
  // user interaction does not allow to call either of those, calling
  // PresaveGeneratedPassword alone is a legitimate thing to do to avoid data
  // loss.
  virtual void PresaveGeneratedPassword(
      const autofill::PasswordForm& generated) = 0;

  // Undo PresaveGeneratedPassword, e.g., when the user deletes the generated
  // password.
  virtual void RemovePresavedPassword() = 0;

  // Creates a new FormSaver with the same state as |*this|.
  virtual std::unique_ptr<FormSaver> Clone() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormSaver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
