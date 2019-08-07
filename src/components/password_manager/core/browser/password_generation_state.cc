// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_state.h"

#include <utility>

#include "base/time/default_clock.h"
#include "components/password_manager/core/browser/form_saver.h"

namespace password_manager {

using autofill::PasswordForm;

PasswordGenerationState::PasswordGenerationState(FormSaver* form_saver)
    : form_saver_(form_saver), clock_(new base::DefaultClock) {}

PasswordGenerationState::~PasswordGenerationState() = default;

std::unique_ptr<PasswordGenerationState> PasswordGenerationState::Clone(
    FormSaver* form_saver) const {
  auto clone = std::make_unique<PasswordGenerationState>(form_saver);
  clone->presaved_ = presaved_;
  return clone;
}

void PasswordGenerationState::PresaveGeneratedPassword(PasswordForm generated) {
  DCHECK(!generated.password_value.empty());
  generated.date_created = clock_->Now();
  if (presaved_) {
    form_saver_->UpdateReplace(generated, {} /* matches */,
                               base::string16() /* old_password */,
                               presaved_.value() /* old_primary_key */);
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
    PasswordForm generated,
    const std::vector<const autofill::PasswordForm*>& matches,
    const base::string16& old_password) {
  DCHECK(presaved_);
  generated.preferred = true;
  generated.date_created = clock_->Now();
  form_saver_->UpdateReplace(generated, matches, old_password,
                             presaved_.value() /* old_primary_key */);
}

}  // namespace password_manager
