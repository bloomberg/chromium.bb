// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FILLING_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FILLING_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"

namespace autofill {
struct PasswordForm;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordFormMetricsRecorder;

// Enum detailing the browser process' best belief what kind of credential
// filling is used in the renderer for a given password form.
//
// NOTE: The renderer can still decide not to fill due to reasons that are only
// known to it, thus this enum contains only probable filling kinds. Due to the
// inherent inaccuracy DO NOT record this enum to UMA.
enum class LikelyFormFilling {
  // There are no credentials to fill.
  kNoFilling,
  // The form is rendered with the best matching credential filled in.
  kFillOnPageLoad,
  // The form requires an active selection of the username before passwords
  // are filled.
  kFillOnAccountSelect,
  // The form is rendered with initial account suggestions, but no credential
  // is filled in.
  kShowInitialAccountSuggestions,
};

LikelyFormFilling SendFillInformationToRenderer(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver,
    const autofill::PasswordForm& observed_form,
    const std::vector<const autofill::PasswordForm*>& best_matches,
    const std::vector<const autofill::PasswordForm*>& federated_matches,
    const autofill::PasswordForm* preferred_match,
    PasswordFormMetricsRecorder* metrics_recorder);

// Create a PasswordFormFillData structure in preparation for filling a form
// identified by |form_on_page|, with credentials from |preferred_match| and
// |matches|. |preferred_match| should equal to one of matches.
// If |wait_for_username| is true then fill on account select will be used.
autofill::PasswordFormFillData CreatePasswordFormFillData(
    const autofill::PasswordForm& form_on_page,
    const std::vector<const autofill::PasswordForm*>& matches,
    const autofill::PasswordForm& preferred_match,
    bool wait_for_username);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_FILLING_H_
