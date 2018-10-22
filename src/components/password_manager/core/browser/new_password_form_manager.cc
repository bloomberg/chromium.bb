// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::ValueElementPair;
using base::TimeDelta;
using base::TimeTicks;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

bool NewPasswordFormManager::wait_for_server_predictions_for_filling_ = true;

namespace {

constexpr TimeDelta kMaxFillingDelayForServerPerdictions =
    TimeDelta::FromMilliseconds(500);

// Helper function for calling form parsing and logging results if logging is
// active.
std::unique_ptr<PasswordForm> ParseFormAndMakeLogging(
    PasswordManagerClient* client,
    const FormData& form,
    const base::Optional<FormPredictions>& predictions,
    FormParsingMode mode) {
  std::unique_ptr<PasswordForm> password_form =
      ParseFormData(form, predictions ? &predictions.value() : nullptr, mode);

  if (password_manager_util::IsLoggingActive(client)) {
    BrowserSavePasswordProgressLogger logger(client->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form)
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
  }
  return password_form;
}

ValueElementPair PasswordToSave(const PasswordForm& form) {
  if (form.new_password_element.empty() || form.new_password_value.empty())
    return {form.password_value, form.password_element};
  return {form.new_password_value, form.new_password_element};
}


// Copies field properties masks from the form |from| to the form |to|.
void CopyFieldPropertiesMasks(const FormData& from, FormData* to) {
  // Skip copying if the number of fields is different.
  if (from.fields.size() != to->fields.size())
    return;

  for (size_t i = 0; i < from.fields.size(); ++i) {
    to->fields[i].properties_mask =
        to->fields[i].name == from.fields[i].name
            ? from.fields[i].properties_mask
            : autofill::FieldPropertiesFlags::ERROR_OCCURRED;
  }
}

}  // namespace

NewPasswordFormManager::NewPasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form,
    FormFetcher* form_fetcher)
    : client_(client),
      driver_(driver),
      observed_form_(observed_form),
      owned_form_fetcher_(
          form_fetcher ? nullptr
                       : std::make_unique<FormFetcherImpl>(
                             PasswordStore::FormDigest(observed_form),
                             client_,
                             true /* should_migrate_http_passwords */,
                             true /* should_query_suppressed_https_forms */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      // TODO(https://crbug.com/831123): set correctly
      // |is_possible_change_password_form| in |votes_uploader_| constructor
      votes_uploader_(client, false /* is_possible_change_password_form */),
      weak_ptr_factory_(this) {
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      client_->IsMainFrameSecure(), client_->GetUkmSourceId());
  metrics_recorder_->RecordFormSignature(CalculateFormSignature(observed_form));

  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);

  // The following code is for development and debugging purposes.
  // TODO(https://crbug.com/831123): remove it when NewPasswordFormManager will
  // be production ready.
  if (password_manager_util::IsLoggingActive(client_))
    ParseFormAndMakeLogging(client_, observed_form_, predictions_,
                            FormParsingMode::FILLING);
}
NewPasswordFormManager::~NewPasswordFormManager() = default;

bool NewPasswordFormManager::DoesManage(
    const autofill::FormData& form,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;
  if (observed_form_.is_form_tag != form.is_form_tag)
    return false;
  // All unowned input elements are considered as one synthetic form.
  if (!observed_form_.is_form_tag && !form.is_form_tag)
    return true;
  return observed_form_.unique_renderer_id == form.unique_renderer_id;
}

bool NewPasswordFormManager::IsEqualToSubmittedForm(
    const autofill::FormData& form) const {
  if (!is_submitted_)
    return false;
  if (form.action == submitted_form_.action)
    return true;
  // TODO(https://crbug.com/831123): Implement other checks from a function
  // IsPasswordFormReappeared from password_manager.cc.
  return false;
}

FormFetcher* NewPasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

const GURL& NewPasswordFormManager::GetOrigin() const {
  return observed_form_.origin;
}

const std::map<base::string16, const PasswordForm*>&
NewPasswordFormManager::GetBestMatches() const {
  return best_matches_;
}

const PasswordForm& NewPasswordFormManager::GetPendingCredentials() const {
  return pending_credentials_;
}

metrics_util::CredentialSourceType
NewPasswordFormManager::GetCredentialSource() {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* NewPasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

const std::vector<const PasswordForm*>&
NewPasswordFormManager::GetBlacklistedMatches() const {
  return blacklisted_matches_;
}

bool NewPasswordFormManager::IsBlacklisted() const {
  return !blacklisted_matches_.empty();
}

bool NewPasswordFormManager::IsPasswordOverridden() const {
  return password_overridden_;
}

const PasswordForm* NewPasswordFormManager::GetPreferredMatch() const {
  return preferred_match_;
}

// TODO(https://crbug.com/831123): Implement all methods from
// PasswordFormManagerForUI.
void NewPasswordFormManager::Save() {}
void NewPasswordFormManager::Update(const PasswordForm& credentials_to_update) {
}
void NewPasswordFormManager::UpdateUsername(
    const base::string16& new_username) {}
void NewPasswordFormManager::UpdatePasswordValue(
    const base::string16& new_password) {}
void NewPasswordFormManager::OnNopeUpdateClicked() {}
void NewPasswordFormManager::OnNeverClicked() {}
void NewPasswordFormManager::OnNoInteraction(bool is_update) {}
void NewPasswordFormManager::PermanentlyBlacklist() {}
void NewPasswordFormManager::OnPasswordsRevealed() {}

bool NewPasswordFormManager::IsNewLogin() const {
  return is_new_login_;
}

bool NewPasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.is_public_suffix_match;
}

bool NewPasswordFormManager::HasGeneratedPassword() const {
  return has_generated_password_;
}
bool NewPasswordFormManager::IsPossibleChangePasswordFormWithoutUsername()
    const {
  // TODO(https://crbug.com/831123): Implement as in PasswordFormManager.
  return false;
}
bool NewPasswordFormManager::RetryPasswordFormPasswordUpdate() const {
  // TODO(https://crbug.com/831123): Implement as in PasswordFormManager.
  return false;
}

void NewPasswordFormManager::ProcessMatches(
    const std::vector<const PasswordForm*>& non_federated,
    size_t filtered_count) {
  received_stored_credentials_time_ = TimeTicks::Now();
  std::vector<const PasswordForm*> matches;
  std::copy_if(non_federated.begin(), non_federated.end(),
               std::back_inserter(matches), [](const PasswordForm* form) {
                 return !form->blacklisted_by_user &&
                        form->scheme == PasswordForm::SCHEME_HTML;
               });

  password_manager_util::FindBestMatches(matches, &best_matches_,
                                         &not_best_matches_, &preferred_match_);

  // Copy out blacklisted matches.
  blacklisted_matches_.clear();
  std::copy_if(
      non_federated.begin(), non_federated.end(),
      std::back_inserter(blacklisted_matches_), [](const PasswordForm* form) {
        return form->blacklisted_by_user && !form->is_public_suffix_match;
      });

  autofills_left_ = kMaxTimesAutofill;

  if (predictions_ || !wait_for_server_predictions_for_filling_) {
    ReportTimeBetweenStoreAndServerUMA();
    Fill();
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NewPasswordFormManager::Fill,
                       weak_ptr_factory_.GetWeakPtr()),
        kMaxFillingDelayForServerPerdictions);
  }
}

bool NewPasswordFormManager::SetSubmittedFormIfIsManaged(
    const autofill::FormData& submitted_form,
    const PasswordManagerDriver* driver) {
  if (!DoesManage(submitted_form, driver))
    return false;
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  CreatePendingCredentials();
  return true;
}

void NewPasswordFormManager::ProcessServerPredictions(
    const std::vector<FormStructure*>& predictions) {
  FormSignature observed_form_signature =
      CalculateFormSignature(observed_form_);
  for (const FormStructure* form_predictions : predictions) {
    if (form_predictions->form_signature() != observed_form_signature)
      continue;
    ReportTimeBetweenStoreAndServerUMA();
    predictions_ = ConvertToFormPredictions(*form_predictions);
    Fill();
    break;
  }
}

void NewPasswordFormManager::Fill() {
  if (autofills_left_ <= 0)
    return;
  autofills_left_--;

  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  std::unique_ptr<PasswordForm> observed_password_form =
      ParseFormAndMakeLogging(client_, observed_form_, predictions_,
                              FormParsingMode::FILLING);
  if (!observed_password_form)
    return;

  RecordMetricOnCompareParsingResult(*observed_password_form);

  // TODO(https://crbug.com/831123). Move this lines to the beginning of the
  // function when the old parsing is removed.
  if (!driver_)
    return;

  // TODO(https://crbug.com/831123). Implement correct treating of federated
  // matches.
  std::vector<const PasswordForm*> federated_matches;
  SendFillInformationToRenderer(*client_, driver_.get(), IsBlacklisted(),
                                *observed_password_form.get(), best_matches_,
                                federated_matches, preferred_match_,
                                metrics_recorder_.get());
}

void NewPasswordFormManager::RecordMetricOnCompareParsingResult(
    const PasswordForm& parsed_form) {
  bool same =
      parsed_form.username_element == old_parsing_result_.username_element &&
      parsed_form.password_element == old_parsing_result_.password_element &&
      parsed_form.new_password_element ==
          old_parsing_result_.new_password_element &&
      parsed_form.confirmation_password_element ==
          old_parsing_result_.confirmation_password_element;
  if (same) {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kSame);
    return;
  }

  // In the old parsing for fields with empty name, placeholders are used. The
  // reason for this is that an empty "..._element" attribute in a PasswordForm
  // means that no corresponding input element exists. The new form parsing sets
  // empty string in that case because renderer ids are used instead of element
  // names for fields identification. Hence in case of anonymous fields, the
  // results will be different for sure. Compare to placeholders and record this
  // case.
  if (old_parsing_result_.username_element ==
          base::ASCIIToUTF16("anonymous_username") ||
      old_parsing_result_.password_element ==
          base::ASCIIToUTF16("anonymous_password") ||
      old_parsing_result_.new_password_element ==
          base::ASCIIToUTF16("anonymous_new_password") ||
      old_parsing_result_.confirmation_password_element ==
          base::ASCIIToUTF16("anonymous_confirmation_password")) {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kAnonymousFields);
  } else {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kDifferent);
  }
}

void NewPasswordFormManager::ReportTimeBetweenStoreAndServerUMA() {
  if (!received_stored_credentials_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PasswordManager.TimeBetweenStoreAndServer",
                        TimeTicks::Now() - received_stored_credentials_time_);
  }
}

void NewPasswordFormManager::CreatePendingCredentials() {
  DCHECK(is_submitted_);
  // TODO(https://crbug.com/831123): Process correctly the case when saved
  // credentials are not received from the store yet.
  std::unique_ptr<PasswordForm> submitted_password_form =
      ParseFormAndMakeLogging(client_, submitted_form_, predictions_,
                              FormParsingMode::SAVING);
  if (!submitted_password_form)
    return;

  ValueElementPair password_to_save(PasswordToSave(*submitted_password_form));

  // Look for the actually submitted credentials in the list of previously saved
  // credentials that were available to autofilling.
  const PasswordForm* saved_form =
      FindBestSavedMatch(submitted_password_form.get());
  if (saved_form) {
    // The user signed in with a login we autofilled.
    pending_credentials_ = *saved_form;
    SetPasswordOverridden(pending_credentials_.password_value !=
                          password_to_save.first);

    if (pending_credentials_.is_public_suffix_match) {
      // If the autofilled credentials were a PSL match or credentials stored
      // from Android apps, store a copy with the current origin and signon
      // realm. This ensures that on the next visit, a precise match is found.
      is_new_login_ = true;
      SetUserAction(password_overridden_ ? UserAction::kOverridePassword
                                         : UserAction::kChoosePslMatch);

      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

      // Update |pending_credentials_| in order to be able correctly save it.
      pending_credentials_.origin = submitted_form_.origin;
      pending_credentials_.signon_realm = submitted_password_form->signon_realm;

      // Normally, the copy of the PSL matched credentials, adapted for the
      // current domain, is saved automatically without asking the user, because
      // the copy likely represents the same account, i.e., the one for which
      // the user already agreed to store a password.
      //
      // However, if the user changes the suggested password, it might indicate
      // that the autofilled credentials and |submitted_password_form|
      // actually correspond to two different accounts (see
      // http://crbug.com/385619). In that case the user should be asked again
      // before saving the password. This is ensured by setting
      // |password_overriden_| on |pending_credentials_| to false.
      //
      // There is still the edge case when the autofilled credentials represent
      // the same account as |submitted_password_form| but the stored password
      // was out of date. In that case, the user just had to manually enter the
      // new password, which is now in |submitted_password_form|. The best
      // thing would be to save automatically, and also update the original
      // credentials. However, we have no way to tell if this is the case.
      // This will likely happen infrequently, and the inconvenience put on the
      // user by asking them is not significant, so we are fine with asking
      // here again.
      if (password_overridden_) {
        pending_credentials_.is_public_suffix_match = false;
        SetPasswordOverridden(false);
      }
    } else {  // Not a PSL match but a match of an already stored credential.
      is_new_login_ = false;
      if (password_overridden_)
        SetUserAction(UserAction::kOverridePassword);
    }
  } else if (!best_matches_.empty() &&
             submitted_password_form->type != PasswordForm::TYPE_API &&
             submitted_password_form->username_value.empty()) {
    // This branch deals with the case that the submitted form has no username
    // element and needs to decide whether to offer to update any credentials.
    // In that case, the user can select any previously stored credential as
    // the one to update, but we still try to find the best candidate.

    // Find the best candidate to select by default in the password update
    // bubble. If no best candidate is found, any one can be offered.
    const PasswordForm* best_update_match =
        FindBestMatchForUpdatePassword(submitted_password_form->password_value);

    // A retry password form is one that consists of only an "old password"
    // field, i.e. one that is not a "new password".
    retry_password_form_password_update_ =
        submitted_password_form->username_value.empty() &&
        submitted_password_form->new_password_value.empty();

    is_new_login_ = false;
    if (best_update_match) {
      // Chose |best_update_match| to be updated.
      pending_credentials_ = *best_update_match;
    } else if (has_generated_password_) {
      // If a password was generated and we didn't find a match, we have to save
      // it in a separate entry since we have to store it but we don't know
      // where.
      CreatePendingCredentialsForNewCredentials(*submitted_password_form,
                                                password_to_save.second);
      is_new_login_ = true;
    } else {
      // We don't have a good candidate to choose as the default credential for
      // the update bubble and the user has to pick one.
      // We set |pending_credentials_| to the bare minimum, which is the correct
      // origin.
      pending_credentials_.origin = submitted_form_.origin;
    }
  } else {
    is_new_login_ = true;
    // No stored credentials can be matched to the submitted form. Offer to
    // save new credentials.
    CreatePendingCredentialsForNewCredentials(*submitted_password_form,
                                              password_to_save.second);
    // Generate username correction votes.
    bool username_correction_found =
        votes_uploader_.FindCorrectedUsernameElement(
            best_matches_, not_best_matches_,
            submitted_password_form->username_value,
            submitted_password_form->password_value);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.UsernameCorrectionFound",
                          username_correction_found);
    if (username_correction_found) {
      metrics_recorder_->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kCorrectedUsernameInForm);
    }
  }

  if (!IsValidAndroidFacetURI(pending_credentials_.signon_realm))
    pending_credentials_.action = submitted_form_.action;

  pending_credentials_.password_value = password_to_save.first;
  pending_credentials_.preferred = true;
  pending_credentials_.form_has_autofilled_value =
      submitted_password_form->form_has_autofilled_value;
  pending_credentials_.all_possible_passwords =
      submitted_password_form->all_possible_passwords;
  CopyFieldPropertiesMasks(submitted_form_, &pending_credentials_.form_data);

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (submitted_password_form->type == PasswordForm::TYPE_API) {
    pending_credentials_.skip_zero_click =
        submitted_password_form->skip_zero_click;
    pending_credentials_.display_name = submitted_password_form->display_name;
    pending_credentials_.federation_origin =
        submitted_password_form->federation_origin;
    pending_credentials_.icon_url = submitted_password_form->icon_url;
    // It's important to override |signon_realm| for federated credentials
    // because it has format "federation://" + origin_host + "/" +
    // federation_host
    pending_credentials_.signon_realm = submitted_password_form->signon_realm;
  }

  if (has_generated_password_)
    pending_credentials_.type = PasswordForm::TYPE_GENERATED;
}

const PasswordForm* NewPasswordFormManager::FindBestMatchForUpdatePassword(
    const base::string16& password) const {
  // This function is called for forms that do not contain a username field.
  // This means that we cannot update credentials based on a matching username
  // and that we may need to show an update prompt.
  if (best_matches_.size() == 1 && !has_generated_password_) {
    // In case the submitted form contained no username but a password, and if
    // the user has only one credential stored, return it as the one that should
    // be updated.
    return best_matches_.begin()->second;
  }
  if (password.empty())
    return nullptr;

  // Return any existing credential that has the same |password| saved already.
  for (const auto& key_value : best_matches_) {
    if (key_value.second->password_value == password)
      return key_value.second;
  }
  return nullptr;
}

const PasswordForm* NewPasswordFormManager::FindBestSavedMatch(
    const PasswordForm* submitted_form) const {
  if (!submitted_form->federation_origin.unique())
    return nullptr;

  // Return form with matching |username_value|.
  auto it = best_matches_.find(submitted_form->username_value);
  if (it != best_matches_.end())
    return it->second;

  // Match Credential API forms only by username. Stop here if nothing was found
  // above.
  if (submitted_form->type == PasswordForm::TYPE_API)
    return nullptr;

  // Verify that the submitted form has no username and no "new password"
  // and bail out with a nullptr otherwise.
  bool submitted_form_has_username = !submitted_form->username_value.empty();
  bool submitted_form_has_new_password_element =
      !submitted_form->new_password_value.empty();
  if (submitted_form_has_username || submitted_form_has_new_password_element)
    return nullptr;

  // At this line we are certain that the submitted form contains only a
  // password field that is not a "new password". Now we can check whether we
  // have a match by password of an already saved credential.
  for (const auto& stored_match : best_matches_) {
    if (stored_match.second->password_value == submitted_form->password_value)
      return stored_match.second;
  }
  return nullptr;
}

void NewPasswordFormManager::CreatePendingCredentialsForNewCredentials(
    const PasswordForm& submitted_password_form,
    const base::string16& password_element) {
  // User typed in a new, unknown username.
  SetUserAction(UserAction::kOverrideUsernameAndPassword);
  // TODO(https://crbug.com/831123): Replace parsing of the observed form with
  // usage of already parsed submitted form.
  std::unique_ptr<PasswordForm> parsed_observed_form = ParseFormAndMakeLogging(
      client_, observed_form_, predictions_, FormParsingMode::FILLING);
  if (!parsed_observed_form)
    return;
  pending_credentials_ = *parsed_observed_form;
  pending_credentials_.username_element =
      submitted_password_form.username_element;
  pending_credentials_.username_value = submitted_password_form.username_value;
  pending_credentials_.other_possible_usernames =
      submitted_password_form.other_possible_usernames;
  pending_credentials_.all_possible_passwords =
      submitted_password_form.all_possible_passwords;

  // The password value will be filled in later, remove any garbage for now.
  pending_credentials_.password_value.clear();
  // The password element should be determined earlier in |PasswordToSave|.
  pending_credentials_.password_element = password_element;
  // The new password's value and element name should be empty.
  pending_credentials_.new_password_value.clear();
  pending_credentials_.new_password_element.clear();
}

void NewPasswordFormManager::SetUserAction(UserAction user_action) {
  user_action_ = user_action;
  metrics_recorder_->SetUserAction(user_action);
}

}  // namespace password_manager
