// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using autofill::FieldDataManager;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::GaiaIdHash;
using autofill::NOT_USERNAME;
using autofill::SINGLE_USERNAME;
using autofill::password_generation::PasswordGenerationType;
using base::TimeTicks;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

bool PasswordFormManager::wait_for_server_predictions_for_filling_ = true;

namespace {

// Returns bit masks with differences in forms attributes which are important
// for parsing. Bits are set according to enum FormDataDifferences.
uint32_t FindFormsDifferences(const FormData& lhs, const FormData& rhs) {
  if (lhs.fields.size() != rhs.fields.size())
    return PasswordFormMetricsRecorder::kFieldsNumber;
  size_t differences_bitmask = 0;
  for (size_t i = 0; i < lhs.fields.size(); ++i) {
    const FormFieldData& lhs_field = lhs.fields[i];
    const FormFieldData& rhs_field = rhs.fields[i];

    if (lhs_field.unique_renderer_id != rhs_field.unique_renderer_id)
      differences_bitmask |= PasswordFormMetricsRecorder::kRendererFieldIDs;

    if (lhs_field.form_control_type != rhs_field.form_control_type)
      differences_bitmask |= PasswordFormMetricsRecorder::kFormControlTypes;

    if (lhs_field.autocomplete_attribute != rhs_field.autocomplete_attribute)
      differences_bitmask |=
          PasswordFormMetricsRecorder::kAutocompleteAttributes;

    if (lhs_field.name != rhs_field.name)
      differences_bitmask |= PasswordFormMetricsRecorder::kFormFieldNames;
  }
  return differences_bitmask;
}

bool FormContainsFieldWithName(const FormData& form,
                               const std::u16string& element) {
  if (element.empty())
    return false;
  for (const auto& field : form.fields) {
    if (base::EqualsCaseInsensitiveASCII(field.name, element))
      return true;
  }
  return false;
}

// Returns whether reparsing server predictions following a form change is
// enabled.
bool IsReparsingServerPredictionsEnabled() {
  return base::FeatureList::IsEnabled(
      features::kReparseServerPredictionsFollowingFormChange);
}

bool IsUsernameFirstFlowFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kUsernameFirstFlow);
}

void LogUsingPossibleUsername(PasswordManagerClient* client,
                              bool is_used,
                              const char* message) {
  if (!password_manager_util::IsLoggingActive(client))
    return;
  BrowserSavePasswordProgressLogger logger(client->GetLogManager());
  logger.LogString(is_used ? Logger::STRING_POSSIBLE_USERNAME_USED
                           : Logger::STRING_POSSIBLE_USERNAME_NOT_USED,
                   message);
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form_data,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : PasswordFormManager(client,
                          observed_form_data,
                          form_fetcher,
                          std::move(password_save_manager),
                          metrics_recorder) {
  driver_ = driver;
  if (driver_)
    driver_id_ = driver->GetId();

  metrics_recorder_->RecordFormSignature(
      CalculateFormSignature(*observed_form()));
  // Do not fetch saved credentials for Chrome sync form, since nor filling nor
  // saving are supported.
  if (owned_form_fetcher_ &&
      !observed_form()->is_gaia_with_skip_save_password_form) {
    owned_form_fetcher_->Fetch();
  }
  votes_uploader_.StoreInitialFieldValues(*observed_form());
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    PasswordFormDigest observed_http_auth_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager)
    : PasswordFormManager(client,
                          observed_http_auth_digest,
                          form_fetcher,
                          std::move(password_save_manager),
                          nullptr /* metrics_recorder */) {
  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
}

PasswordFormManager::~PasswordFormManager() {
  form_fetcher_->RemoveConsumer(this);
}

bool PasswordFormManager::DoesManage(
    autofill::FormRendererId form_renderer_id,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;
  return observed_form()->unique_renderer_id == form_renderer_id;
}

bool PasswordFormManager::IsEqualToSubmittedForm(
    const autofill::FormData& form) const {
  if (!is_submitted_)
    return false;
  if (IsHttpAuth())
    return false;

  if (form.action.is_valid() && !form.is_action_empty &&
      !submitted_form_.is_action_empty &&
      submitted_form_.action == form.action) {
    return true;
  }

  // Match the form if username and password fields are same.
  if (FormContainsFieldWithName(form,
                                parsed_submitted_form_->username_element) &&
      FormContainsFieldWithName(form,
                                parsed_submitted_form_->password_element)) {
    return true;
  }

  // Match the form if the observed username field has the same value as in
  // the submitted form.
  if (!parsed_submitted_form_->username_value.empty()) {
    for (const auto& field : form.fields) {
      if (field.value == parsed_submitted_form_->username_value)
        return true;
    }
  }
  return false;
}

const GURL& PasswordFormManager::GetURL() const {
  return observed_form() ? observed_form()->url : observed_digest()->url;
}

const std::vector<const PasswordForm*>& PasswordFormManager::GetBestMatches()
    const {
  return form_fetcher_->GetBestMatches();
}

std::vector<const PasswordForm*> PasswordFormManager::GetFederatedMatches()
    const {
  return form_fetcher_->GetFederatedMatches();
}

const PasswordForm& PasswordFormManager::GetPendingCredentials() const {
  return password_save_manager_->GetPendingCredentials();
}

metrics_util::CredentialSourceType PasswordFormManager::GetCredentialSource()
    const {
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* PasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

base::span<const InteractionsStats> PasswordFormManager::GetInteractionsStats()
    const {
  return base::make_span(form_fetcher_->GetInteractionsStats());
}

base::span<const InsecureCredential>
PasswordFormManager::GetInsecureCredentials() const {
  return form_fetcher_->GetInsecureCredentials();
}

bool PasswordFormManager::IsBlocklisted() const {
  return form_fetcher_->IsBlocklisted() || newly_blocklisted_;
}

bool PasswordFormManager::WasUnblocklisted() const {
  return was_unblocklisted_while_on_page_;
}

bool PasswordFormManager::IsMovableToAccountStore() const {
  DCHECK(
      client_->GetPasswordFeatureManager()->ShouldShowAccountStorageBubbleUi())
      << "Ensure that the client supports moving passwords for this user!";
  signin::IdentityManager* identity_manager = client_->GetIdentityManager();
  DCHECK(identity_manager);
  const std::string gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  DCHECK(!gaia_id.empty()) << "Cannot move without signed in user";

  const std::u16string& username = GetPendingCredentials().username_value;
  const std::u16string& password = GetPendingCredentials().password_value;
  // If no match in the profile store with the same username and password exist,
  // then there is nothing to move.
  auto is_movable = [&](const PasswordForm* match) {
    return !match->IsUsingAccountStore() && match->username_value == username &&
           match->password_value == password;
  };
  return base::ranges::any_of(form_fetcher_->GetBestMatches(), is_movable) &&
         !form_fetcher_->IsMovingBlocked(GaiaIdHash::FromGaiaId(gaia_id),
                                         username);
}

void PasswordFormManager::Save() {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(!client_->IsIncognito());
  if (IsBlocklisted()) {
    password_save_manager_->Unblocklist(ConstructObservedFormDigest());
    newly_blocklisted_ = false;
  }

  password_save_manager_->Save(observed_form(), *parsed_submitted_form_);

  client_->UpdateFormManagers();
}

void PasswordFormManager::Update(const PasswordForm& credentials_to_update) {
  password_save_manager_->Update(credentials_to_update, observed_form(),
                                 *parsed_submitted_form_);

  client_->UpdateFormManagers();
}

void PasswordFormManager::OnUpdateUsernameFromPrompt(
    const std::u16string& new_username) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->username_value = new_username;
  parsed_submitted_form_->username_element.clear();

  metrics_recorder_->set_username_updated_in_bubble(true);

  if (!new_username.empty()) {
    // Try to find `new_username` in the usernames `parsed_submitted_form_`
    // knows about. Set `votes_uploader_`'s UsernameChangeState depending on
    // whether the username is present or not. Also set `username_element` if it
    // is a known username.
    const auto& possible_usernames =
        parsed_submitted_form_->all_possible_usernames;
    auto possible_username_it = base::ranges::find(
        possible_usernames, new_username, &ValueElementPair::first);

    if (possible_username_it != possible_usernames.end()) {
      parsed_submitted_form_->username_element = possible_username_it->second;
      votes_uploader_.set_username_change_state(
          VotesUploader::UsernameChangeState::kChangedToKnownValue);
    } else {
      votes_uploader_.set_username_change_state(
          VotesUploader::UsernameChangeState::kChangedToUnknownValue);
    }
  }

  CreatePendingCredentials();
}

void PasswordFormManager::OnUpdatePasswordFromPrompt(
    const std::u16string& new_password) {
  DCHECK(parsed_submitted_form_);
  parsed_submitted_form_->password_value = new_password;
  parsed_submitted_form_->password_element.clear();

  // The user updated a password from the prompt. It means that heuristics were
  // wrong. So clear new password, since it is likely wrong.
  parsed_submitted_form_->new_password_value.clear();
  parsed_submitted_form_->new_password_element.clear();

  for (const ValueElementPair& pair :
       parsed_submitted_form_->all_possible_passwords) {
    if (pair.first == new_password) {
      parsed_submitted_form_->password_element = pair.second;
      break;
    }
  }

  CreatePendingCredentials();
}

void PasswordFormManager::UpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event) {
  parsed_submitted_form_->form_data.submission_event = event;
  parsed_submitted_form_->submission_event = event;
  password_save_manager_->UpdateSubmissionIndicatorEvent(event);
}

void PasswordFormManager::OnNopeUpdateClicked() {
  votes_uploader_.UploadPasswordVote(*parsed_submitted_form_,
                                     *parsed_submitted_form_,
                                     autofill::NOT_NEW_PASSWORD, std::string());
}

void PasswordFormManager::OnNeverClicked() {
  // |UNKNOWN_TYPE| is sent in order to record that a generation popup was
  // shown and ignored.
  votes_uploader_.UploadPasswordVote(*parsed_submitted_form_,
                                     *parsed_submitted_form_,
                                     autofill::UNKNOWN_TYPE, std::string());
  Blocklist();
}

void PasswordFormManager::OnNoInteraction(bool is_update) {
  // |UNKNOWN_TYPE| is sent in order to record that a generation popup was
  // shown and ignored.
  votes_uploader_.UploadPasswordVote(
      *parsed_submitted_form_, *parsed_submitted_form_,
      is_update ? autofill::PROBABLY_NEW_PASSWORD : autofill::UNKNOWN_TYPE,
      std::string());
}

void PasswordFormManager::Blocklist() {
  DCHECK(!client_->IsIncognito());
  password_save_manager_->Blocklist(ConstructObservedFormDigest());
  newly_blocklisted_ = true;
}

PasswordFormDigest PasswordFormManager::ConstructObservedFormDigest() const {
  std::string signon_realm;
  GURL url;
  if (observed_digest()) {
    url = observed_digest()->url;
    // GetSignonRealm is not suitable for http auth credentials.
    signon_realm = IsHttpAuth() ? observed_digest()->signon_realm
                                : GetSignonRealm(observed_digest()->url);
  } else {
    url = observed_form()->url;
    signon_realm = GetSignonRealm(observed_form()->url);
  }
  return PasswordFormDigest(GetScheme(), signon_realm, url);
}

void PasswordFormManager::OnPasswordsRevealed() {
  votes_uploader_.set_has_passwords_revealed_vote(true);
}

void PasswordFormManager::MoveCredentialsToAccountStore() {
  DCHECK(client_->GetPasswordFeatureManager()->IsOptedInForAccountStorage());
  password_save_manager_->MoveCredentialsToAccountStore(
      metrics_util::MoveToAccountStoreTrigger::
          kSuccessfulLoginWithProfileStorePassword);
}

void PasswordFormManager::BlockMovingCredentialsToAccountStore() {
  // Nothing to do if there is no signed in user or the credentials are already
  // blocked for moving.
  if (!IsMovableToAccountStore())
    return;
  const std::string gaia_id =
      client_->GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  // The above call to IsMovableToAccountStore() guarantees there is a signed in
  // user.
  DCHECK(!gaia_id.empty());
  password_save_manager_->BlockMovingToAccountStoreFor(
      GaiaIdHash::FromGaiaId(gaia_id));
}

bool PasswordFormManager::IsNewLogin() const {
  return password_save_manager_->IsNewLogin();
}

FormFetcher* PasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return password_save_manager_->GetPendingCredentials().is_public_suffix_match;
}

void PasswordFormManager::PresaveGeneratedPassword(
    const FormData& form_data,
    const std::u16string& password_value) {
  // TODO(https://crbug.com/831123): Propagate generated password independently
  // of PasswordForm when PasswordForm goes away from the renderer process.
  PresaveGeneratedPasswordInternal(form_data,
                                   password_value /*generated_password*/);
}

void PasswordFormManager::PasswordNoLongerGenerated() {
  if (!HasGeneratedPassword())
    return;

  password_save_manager_->PasswordNoLongerGenerated();
}

bool PasswordFormManager::HasGeneratedPassword() const {
  return password_save_manager_->HasGeneratedPassword();
}

void PasswordFormManager::SetGenerationPopupWasShown(
    PasswordGenerationType type) {
  const bool is_manual_generation = type == PasswordGenerationType::kManual;
  votes_uploader_.set_generation_popup_was_shown(true);
  votes_uploader_.set_is_manual_generation(is_manual_generation);
  metrics_recorder_->SetPasswordGenerationPopupShown(true,
                                                     is_manual_generation);
}

void PasswordFormManager::SetGenerationElement(
    FieldRendererId generation_element) {
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::HasLikelyChangePasswordFormSubmitted() const {
  return parsed_submitted_form_ &&
         parsed_submitted_form_->IsLikelyChangePasswordForm();
}

bool PasswordFormManager::IsPasswordUpdate() const {
  return password_save_manager_->IsPasswordUpdate();
}

base::WeakPtr<PasswordManagerDriver> PasswordFormManager::GetDriver() const {
  return driver_;
}

const PasswordForm* PasswordFormManager::GetSubmittedForm() const {
  return parsed_submitted_form_.get();
}

#if defined(OS_IOS)
void PasswordFormManager::PresaveGeneratedPassword(
    PasswordManagerDriver* driver,
    const FormData& form,
    const std::u16string& generated_password,
    FieldRendererId generation_element) {
  *mutable_observed_form() = form;
  PresaveGeneratedPasswordInternal(form, generated_password);
  votes_uploader_.set_generation_element(generation_element);
}

bool PasswordFormManager::UpdateStateOnUserInput(
    FormRendererId form_id,
    FieldRendererId field_id,
    const std::u16string& field_value) {
  if (form_id) {
    if (!observed_form()->is_form_tag ||
        (observed_form()->is_form_tag &&
         observed_form()->unique_renderer_id != form_id)) {
      return false;
    }
  } else if (observed_form()->is_form_tag) {
    return false;
  }

  bool form_data_changed = false;
  for (FormFieldData& field : mutable_observed_form()->fields) {
    if (field.unique_renderer_id == field_id) {
      field.value = field_value;
      form_data_changed = true;
      break;
    }
  }

  if (!HasGeneratedPassword())
    return true;

  std::u16string generated_password =
      password_save_manager_->GetGeneratedPassword();
  if (votes_uploader_.get_generation_element() == field_id) {
    generated_password = field_value;
    form_data_changed = true;
  }
  if (form_data_changed)
    PresaveGeneratedPasswordInternal(*observed_form(), generated_password);
  return true;
}

void PasswordFormManager::SetDriver(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  driver_ = driver;
}

void PasswordFormManager::ProvisionallySaveFieldDataManagerInfo(
    const FieldDataManager& field_data_manager,
    const PasswordManagerDriver* driver) {
  bool data_found = false;
  for (FormFieldData& field : mutable_observed_form()->fields) {
    FieldRendererId field_id = field.unique_renderer_id;
    if (!field_data_manager.HasFieldData(field_id))
      continue;
    field.user_input = field_data_manager.GetUserInput(field_id);
    field.properties_mask = field_data_manager.GetFieldPropertiesMask(field_id);
    data_found = true;
  }

  // Provisionally save form and set the manager to be submitted if valid
  // data was recovered.
  if (data_found)
    ProvisionallySave(*observed_form(), driver, nullptr);
}
#endif  // defined(OS_IOS)

void PasswordFormManager::SaveSuggestedUsernameValueToVotesUploader() {
  votes_uploader_.set_suggested_username(
      GetPendingCredentials().username_value);
}

std::unique_ptr<PasswordFormManager> PasswordFormManager::Clone() {
  // Fetcher is cloned to avoid re-fetching data from PasswordStore.
  std::unique_ptr<FormFetcher> fetcher = form_fetcher_->Clone();

  // Some data is filled through the constructor. No PasswordManagerDriver is
  // needed, because the UI does not need any functionality related to the
  // renderer process, to which the driver serves as an interface.
  auto result = base::WrapUnique(new PasswordFormManager(
      client_, observed_form_or_digest_, fetcher.get(),
      password_save_manager_->Clone(), metrics_recorder_));

  // The constructor only can take a weak pointer to the fetcher, so moving the
  // owning one needs to happen explicitly.
  result->owned_form_fetcher_ = std::move(fetcher);

  // These data members all satisfy:
  //   (1) They could have been changed by |*this| between its construction and
  //       calling Clone().
  //   (2) They are potentially used in the clone as the clone is used in the UI
  //       code.
  //   (3) They are not changed during OnFetchCompleted, triggered at some point
  //   by the
  //       cloned FormFetcher.
  result->votes_uploader_ = votes_uploader_;

  if (parser_.predictions())
    result->parser_.set_predictions(*parser_.predictions());

  if (parsed_submitted_form_) {
    result->parsed_submitted_form_ =
        std::make_unique<PasswordForm>(*parsed_submitted_form_);
  }
  result->is_submitted_ = is_submitted_;
  result->password_save_manager_->Init(result->client_, result->form_fetcher_,
                                       result->metrics_recorder_,
                                       &result->votes_uploader_);
  return result;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    std::unique_ptr<PasswordForm> saved_form,
    std::unique_ptr<FormFetcher> form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager)
    : PasswordFormManager(client,
                          PasswordFormDigest(*saved_form),
                          form_fetcher.get(),
                          std::move(password_save_manager),
                          nullptr /* metrics_recorder */) {
  parsed_submitted_form_ = std::move(saved_form);
  is_submitted_ = true;
  owned_form_fetcher_ = std::move(form_fetcher);
  owned_form_fetcher_->Fetch();
}

void PasswordFormManager::DelayFillForServerSidePredictions() {
  waiting_for_server_predictions_ = true;

  weak_ptr_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordFormManager::Fill,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxFillingDelayForServerPredictions);
}

void PasswordFormManager::OnFetchCompleted() {
  received_stored_credentials_time_ = TimeTicks::Now();

  newly_blocklisted_ = false;
  autofills_left_ = kMaxTimesAutofill;

  if (IsCredentialAPISave()) {
    // This is saving with credential API, there is no form to fill, so no
    // filling required.
    return;
  }

  client_->UpdateCredentialCache(url::Origin::Create(GetURL()),
                                 form_fetcher_->GetBestMatches(),
                                 form_fetcher_->IsBlocklisted());

  if (is_submitted_)
    CreatePendingCredentials();

  if (IsHttpAuth()) {
    // No server prediction for http auth, so no need to wait.
    FillHttpAuth();
  } else if (parser_.predictions() ||
             !wait_for_server_predictions_for_filling_) {
    ReportTimeBetweenStoreAndServerUMA();
    Fill();
  } else if (!waiting_for_server_predictions_) {
    DelayFillForServerSidePredictions();
  }
}

void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(is_submitted_);
  if (!parsed_submitted_form_)
    return;

  password_save_manager_->CreatePendingCredentials(
      *parsed_submitted_form_, observed_form(), submitted_form_, IsHttpAuth(),
      IsCredentialAPISave());
}

void PasswordFormManager::ResetState() {
  parsed_submitted_form_.reset();
  submitted_form_ = FormData();
  password_save_manager_->ResetPendingCredentials();
  is_submitted_ = false;
}

bool PasswordFormManager::ProvisionallySave(
    const FormData& submitted_form,
    const PasswordManagerDriver* driver,
    const PossibleUsernameData* possible_username) {
  DCHECK(DoesManage(submitted_form.unique_renderer_id, driver));
  DCHECK(client_->IsSavingAndFillingEnabled(submitted_form.url));
  std::unique_ptr<PasswordForm> parsed_submitted_form =
      ParseFormAndMakeLogging(submitted_form, FormDataParser::Mode::kSaving);
  RecordMetricOnReadonly(parser_.readonly_status(), !!parsed_submitted_form,
                         FormDataParser::Mode::kSaving);

  bool have_password_to_save =
      parsed_submitted_form &&
      parsed_submitted_form->HasNonEmptyPasswordValue();

  if (!have_password_to_save) {
    // In case of error during parsing, reset the state.
    ResetState();
    return false;
  }

  parsed_submitted_form_ = std::move(parsed_submitted_form);
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  CalculateFillingAssistanceMetric(submitted_form);
  metrics_recorder_->set_possible_username_used(false);
  votes_uploader_.clear_single_username_vote_data();

  // TODO(crbug.com/959776): Reset possible username after it's used.
  if (IsUsernameFirstFlowFeatureEnabled() &&
      parsed_submitted_form_->username_value.empty() &&
      !parsed_submitted_form_->password_value.empty()) {
    if (IsPossibleSingleUsernameAvailable(possible_username)) {
      // Suggest the possible username value in a prompt if the server confirmed
      // it is a single username field. Otherwise, |possible_username| is used
      // only for voting.
      if (possible_username->HasSingleUsernameServerPrediction()) {
        parsed_submitted_form_->username_value = possible_username->value;
        metrics_recorder_->set_possible_username_used(true);
        LogUsingPossibleUsername(
            client_, /*is_used*/ true,
            "Valid possible username, populated in prompt");
      } else {
        LogUsingPossibleUsername(
            client_, /*is_used*/ true,
            "Valid possible username, not populated in prompt");
      }
      votes_uploader_.set_single_username_vote_data(
          possible_username->renderer_id, possible_username->value,
          possible_username->form_predictions.value_or(FormPredictions()),
          form_fetcher_->GetBestMatches());
    } else {  // !IsPossibleSingleUsernameAvailable(possible_username)
      // If no single username typing preceded single password typing, set
      // empty single username vote data for the fallback classifier.
      votes_uploader_.set_single_username_vote_data(
          FieldRendererId(), std::u16string(), FormPredictions(),
          form_fetcher_->GetBestMatches());
    }
  }
  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::ProvisionallySaveHttpAuthForm(
    const PasswordForm& submitted_form) {
  if (!IsHttpAuth())
    return false;
  if (*observed_digest() != PasswordFormDigest(submitted_form))
    return false;

  parsed_submitted_form_ = std::make_unique<PasswordForm>(submitted_form);
  is_submitted_ = true;
  CreatePendingCredentials();
  return true;
}

bool PasswordFormManager::IsHttpAuth() const {
  return GetScheme() != PasswordForm::Scheme::kHtml;
}

bool PasswordFormManager::IsCredentialAPISave() const {
  return observed_digest() && !IsHttpAuth();
}

PasswordForm::Scheme PasswordFormManager::GetScheme() const {
  return observed_digest() ? observed_digest()->scheme
                           : PasswordForm::Scheme::kHtml;
}

void PasswordFormManager::ProcessServerPredictions(
    const std::map<FormSignature, FormPredictions>& predictions) {
  if (parser_.predictions()) {
    // This method might be called multiple times. No need to process
    // predictions again.
    return;
  }
  UpdatePredictionsForObservedForm(predictions);
  if (parser_.predictions())
    Fill();
}

void PasswordFormManager::Fill() {
  if (!driver_)
    return;

  waiting_for_server_predictions_ = false;

  if (form_fetcher_->GetState() == FormFetcher::State::WAITING)
    return;

  if (autofills_left_ <= 0)
    return;
  autofills_left_--;

  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  std::unique_ptr<PasswordForm> observed_password_form =
      ParseFormAndMakeLogging(*observed_form(), FormDataParser::Mode::kFilling);
  RecordMetricOnReadonly(parser_.readonly_status(), !!observed_password_form,
                         FormDataParser::Mode::kFilling);
  if (!observed_password_form)
    return;

  if (observed_password_form->is_new_password_reliable && !IsBlocklisted()) {
    driver_->FormEligibleForGenerationFound({
#if defined(OS_IOS)
      .form_renderer_id = observed_password_form->form_data.unique_renderer_id,
#endif
      .new_password_renderer_id =
          observed_password_form->new_password_element_renderer_id,
      .confirmation_password_renderer_id =
          observed_password_form->confirmation_password_element_renderer_id,
    });
  }

#if defined(OS_IOS)
  // Filling on username first flow is not supported on iOS.
  if (observed_password_form->IsSingleUsername())
    return;
#endif

  SendFillInformationToRenderer(
      client_, driver_.get(), *observed_password_form.get(),
      form_fetcher_->GetBestMatches(), form_fetcher_->GetFederatedMatches(),
      form_fetcher_->GetPreferredMatch(), form_fetcher_->IsBlocklisted(),
      metrics_recorder_.get());
}

void PasswordFormManager::FillForm(
    const FormData& observed_form_data,
    const std::map<FormSignature, FormPredictions>& predictions) {
  uint32_t differences_bitmask =
      FindFormsDifferences(*observed_form(), observed_form_data);
  metrics_recorder_->RecordFormChangeBitmask(differences_bitmask);

  bool new_predictions_available = false;
  if (differences_bitmask) {
    UpdateFormManagerWithFormChanges(observed_form_data, predictions);
    new_predictions_available =
        parser_.predictions() && IsReparsingServerPredictionsEnabled();
  }
  // Fill the form if relevant form predictions were found or if the
  // manager is not waiting for new server predictions.
  if (new_predictions_available || !waiting_for_server_predictions_)
    Fill();
}

void PasswordFormManager::OnGeneratedPasswordAccepted(
    FormData form_data,
    autofill::FieldRendererId generation_element_id,
    const std::u16string& password) {
  // Find the generating element to update its value. The parser needs a non
  // empty value.
  auto it = std::find_if(form_data.fields.begin(), form_data.fields.end(),
                         [generation_element_id](const auto& field_data) {
                           return generation_element_id ==
                                  field_data.unique_renderer_id;
                         });
  // The parameters are coming from the renderer and can't be trusted.
  if (it == form_data.fields.end())
    return;
  it->value = password;
  std::unique_ptr<PasswordForm> parsed_form =
      ParseFormAndMakeLogging(form_data, FormDataParser::Mode::kSaving);
  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form = std::make_unique<PasswordForm>();
    parsed_form->url = form_data.url;
    parsed_form->signon_realm = GetSignonRealm(form_data.url);
  }
  parsed_form->password_value = password;
  password_save_manager_->GeneratedPasswordAccepted(*parsed_form, driver_);
}

void PasswordFormManager::MarkWasUnblocklisted() {
  was_unblocklisted_while_on_page_ = true;
}

PasswordFormManager::PasswordFormManager(
    PasswordManagerClient* client,
    FormOrDigest observed_form_or_digest,
    FormFetcher* form_fetcher,
    std::unique_ptr<PasswordSaveManager> password_save_manager,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder)
    : client_(client),
      observed_form_or_digest_(std::move(observed_form_or_digest)),
      metrics_recorder_(metrics_recorder),
      owned_form_fetcher_(form_fetcher
                              ? nullptr
                              : std::make_unique<FormFetcherImpl>(
                                    observed_digest()
                                        ? *observed_digest()
                                        : PasswordFormDigest(*observed_form()),
                                    client_,
                                    true /* should_migrate_http_passwords */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      password_save_manager_(std::move(password_save_manager)),
      // TODO(https://crbug.com/831123): set correctly
      // |is_possible_change_password_form| in |votes_uploader_| constructor
      votes_uploader_(client, false /* is_possible_change_password_form */) {
  form_fetcher_->AddConsumer(this);
  if (!metrics_recorder_) {
    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_->IsCommittedMainFrameSecure(), client_->GetUkmSourceId(),
        client_->GetPrefs());
  }
  password_save_manager_->Init(client_, form_fetcher_, metrics_recorder_,
                               &votes_uploader_);
  base::UmaHistogramEnumeration("PasswordManager.FormVisited.PerProfileType",
                                client_->GetProfileType());
}

void PasswordFormManager::RecordMetricOnReadonly(
    FormDataParser::ReadonlyPasswordFields readonly_status,
    bool parsing_successful,
    FormDataParser::Mode mode) {
  // The reported value is combined of the |readonly_status| shifted by one bit
  // to the left, and the success bit put in the least significant bit. Note:
  // C++ guarantees that bool->int conversions map false to 0 and true to 1.
  uint64_t value = static_cast<uint64_t>(parsing_successful) +
                   (static_cast<uint64_t>(readonly_status) << 1);
  switch (mode) {
    case FormDataParser::Mode::kSaving:
      metrics_recorder_->RecordReadonlyWhenSaving(value);
      break;
    case FormDataParser::Mode::kFilling:
      metrics_recorder_->RecordReadonlyWhenFilling(value);
      break;
  }
}

void PasswordFormManager::ReportTimeBetweenStoreAndServerUMA() {
  if (!received_stored_credentials_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("PasswordManager.TimeBetweenStoreAndServer",
                        TimeTicks::Now() - received_stored_credentials_time_);
  }
}

void PasswordFormManager::FillHttpAuth() {
  DCHECK(IsHttpAuth());
  if (!form_fetcher_->GetPreferredMatch())
    return;
  client_->AutofillHttpAuth(*form_fetcher_->GetPreferredMatch(), this);
}

std::unique_ptr<PasswordForm> PasswordFormManager::ParseFormAndMakeLogging(
    const FormData& form,
    FormDataParser::Mode mode) {
  std::unique_ptr<PasswordForm> password_form = parser_.Parse(form, mode);

  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form)
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
  }
  return password_form;
}

void PasswordFormManager::PresaveGeneratedPasswordInternal(
    const FormData& form,
    const std::u16string& generated_password) {
  std::unique_ptr<PasswordForm> parsed_form =
      ParseFormAndMakeLogging(form, FormDataParser::Mode::kSaving);

  if (!parsed_form) {
    // Create a password form with a minimum data.
    parsed_form = std::make_unique<PasswordForm>();
    parsed_form->url = form.url;
    parsed_form->signon_realm = GetSignonRealm(form.url);
  }
  // Set |password_value| to the generated password in order to ensure that
  // the generated password is saved.
  parsed_form->password_value = generated_password;

  password_save_manager_->PresaveGeneratedPassword(std::move(*parsed_form));
}

void PasswordFormManager::CalculateFillingAssistanceMetric(
    const FormData& submitted_form) {
  std::set<std::pair<std::u16string, PasswordForm::Store>> saved_usernames;
  std::set<std::pair<std::u16string, PasswordForm::Store>> saved_passwords;

  for (auto* saved_form : form_fetcher_->GetNonFederatedMatches()) {
    // Saved credentials might have empty usernames which are not interesting
    // for filling assistance metric.
    if (!saved_form->username_value.empty())
      saved_usernames.emplace(saved_form->username_value, saved_form->in_store);
    saved_passwords.emplace(saved_form->password_value, saved_form->in_store);
  }

  metrics_recorder_->CalculateFillingAssistanceMetric(
      submitted_form, saved_usernames, saved_passwords, IsBlocklisted(),
      form_fetcher_->GetInteractionsStats(),
      client_->GetPasswordFeatureManager()
          ->ComputePasswordAccountStorageUsageLevel());
}

bool PasswordFormManager::IsPossibleSingleUsernameAvailable(
    const PossibleUsernameData* possible_username) const {
  if (!possible_username) {
    LogUsingPossibleUsername(client_, /*is_used*/ false, "Null");
    return false;
  }

  // The username form and password forms signon realms must be the same.
  if (parsed_submitted_form_->signon_realm != possible_username->signon_realm) {
    LogUsingPossibleUsername(client_, /*is_used*/ false, "Different domains");
    return false;
  }

  if (possible_username->value.empty()) {
    LogUsingPossibleUsername(client_, /*is_used*/ false,
                             "Empty possible username value");
    return false;
  }

  // PasswordManager does not upload votes for fields that have no names or
  // ids to avoid aggregation of multiple unrelated fields during single
  // username detection. (crbug.com/1209143)
  if (possible_username->field_name.empty()) {
    LogUsingPossibleUsername(client_, /*is_used*/ false, "Empty field name");
    return false;
  }

  if (possible_username->IsStale()) {
    LogUsingPossibleUsername(client_, /*is_used*/ false,
                             "Possible username data expired");
    return false;
  }

  // The username candidate field should not be in |observed_form()|, otherwise
  // that is a task of FormParser to choose it from |observed_form()|.
  if (possible_username->driver_id == driver_id_) {
    for (const auto& field : observed_form()->fields) {
      if (field.unique_renderer_id == possible_username->renderer_id) {
        LogUsingPossibleUsername(client_, /*is_used*/ false, "Same form");
        return false;
      }
    }
  }

  return true;
}

void PasswordFormManager::UpdatePredictionsForObservedForm(
    const std::map<FormSignature, FormPredictions>& predictions) {
  FormSignature observed_form_signature =
      CalculateFormSignature(*observed_form());
  auto it = predictions.find(observed_form_signature);
  if (it == predictions.end())
    return;

  ReportTimeBetweenStoreAndServerUMA();
  parser_.set_predictions(it->second);
}

void PasswordFormManager::UpdateFormManagerWithFormChanges(
    const FormData& observed_form_data,
    const std::map<FormSignature, FormPredictions>& predictions) {
  *mutable_observed_form() = observed_form_data;
  if (!IsReparsingServerPredictionsEnabled())
    return;

  // If the observed form has changed, it might be autofilled again.
  autofills_left_ = kMaxTimesAutofill;
  parser_.reset_predictions();
  UpdatePredictionsForObservedForm(predictions);

  DelayFillForServerSidePredictions();
}

}  // namespace password_manager
