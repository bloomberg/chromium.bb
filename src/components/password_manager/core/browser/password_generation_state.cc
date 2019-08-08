// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_state.h"

#include <utility>

#include "components/password_manager/core/browser/form_saver.h"

namespace password_manager {

using autofill::PasswordForm;

PasswordGenerationState::PasswordGenerationState(FormSaver* form_saver)
    : form_saver_(form_saver) {}

PasswordGenerationState::~PasswordGenerationState() = default;

std::unique_ptr<PasswordGenerationState> PasswordGenerationState::Clone(
    FormSaver* form_saver) const {
  auto clone = std::make_unique<PasswordGenerationState>(form_saver);
  clone->presaved_ = presaved_;
  return clone;
}

void PasswordGenerationState::PresaveGeneratedPassword(PasswordForm generated) {
  DCHECK(!generated.password_value.empty());
  if (presaved_) {
    form_saver_->Update(generated, {} /* best_matches */,
                        nullptr /* credentials_to_update */,
                        &presaved_.value() /* old_primary_key */);
  } else {
    form_saver_->Save(generated, {} /* matches */,
                      base::string16() /* old_password */);
  }
  presaved_ = std::move(generated);
}

void PasswordGenerationState::PasswordNoLongerGenerated() {
  DCHECK(presaved_);
  form_saver_->Remove(*presaved_);
  presaved_.reset();
}

void PasswordGenerationState::CommitGeneratedPassword(
    const PasswordForm& generated,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<PasswordForm>* credentials_to_update) {
  DCHECK(presaved_);
  form_saver_->Update(generated, best_matches, credentials_to_update,
                      &presaved_.value() /* old_primary_key */);
  presaved_.reset();
}

}  // namespace password_manager
