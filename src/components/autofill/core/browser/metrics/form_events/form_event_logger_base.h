// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

class LogManager;

// Utility to log autofill form events in the relevant histograms depending on
// the presence of server and/or local data.
class FormEventLoggerBase {
 public:
  FormEventLoggerBase(
      const std::string& form_type_name,
      bool is_in_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      LogManager* log_manager);

  inline void set_server_record_type_count(size_t server_record_type_count) {
    server_record_type_count_ = server_record_type_count;
  }

  inline void set_local_record_type_count(size_t local_record_type_count) {
    local_record_type_count_ = local_record_type_count;
  }

  void OnDidInteractWithAutofillableForm(const FormStructure& form,
                                         AutofillSyncSigninState sync_state);

  void OnDidPollSuggestions(const FormFieldData& field,
                            AutofillSyncSigninState sync_state);

  void OnDidParseForm(const FormStructure& form);

  void OnPopupSuppressed(const FormStructure& form, const AutofillField& field);

  void OnUserHideSuggestions(const FormStructure& form,
                             const AutofillField& field);

  virtual void OnDidShowSuggestions(
      const FormStructure& form,
      const AutofillField& field,
      const base::TimeTicks& form_parsed_timestamp,
      AutofillSyncSigninState sync_state,
      bool off_the_record);

  void OnWillSubmitForm(AutofillSyncSigninState sync_state,
                        const FormStructure& form);

  void OnFormSubmitted(AutofillSyncSigninState sync_state,
                       const FormStructure& form);

  void OnTypedIntoNonFilledField();
  void OnEditedAutofilledField();

  // See BrowserAutofillManager::SuggestionContext for the definitions of the
  // AblationGroup parameters.
  void SetAblationStatus(AblationGroup ablation_group,
                         AblationGroup conditional_ablation_group);
  void SetTimeFromInteractionToSubmission(
      base::TimeDelta time_from_interaction_to_submission);

 protected:
  virtual ~FormEventLoggerBase();

  void Log(FormEvent event, const FormStructure& form) const;

  virtual void RecordPollSuggestions() = 0;
  virtual void RecordParseForm() = 0;
  virtual void RecordShowSuggestions() = 0;

  virtual void LogWillSubmitForm(const FormStructure& form);
  virtual void LogFormSubmitted(const FormStructure& form);

  // Only used for UKM backward compatibility since it depends on IsCreditCard.
  // TODO (crbug.com/925913): Remove IsCreditCard from UKM logs amd replace with
  // |form_type_name_|.
  virtual void LogUkmInteractedWithForm(FormSignature form_signature);

  virtual void OnSuggestionsShownOnce(const FormStructure& form) {}
  virtual void OnSuggestionsShownSubmittedOnce(const FormStructure& form) {}

  // Logs |event| in a histogram prefixed with |name| according to the
  // FormEventLogger type and |form|. For example, in the address context, it
  // may be useful to analyze metrics for forms (A) with only name and address
  // fields and (B) with only name and phone fields separately.
  virtual void OnLog(const std::string& name,
                     FormEvent event,
                     const FormStructure& form) const {}

  // Records UMA metrics on the funnel and key metrics. This is not virtual
  // because it is called in the destructor.
  void RecordFunnelAndKeyMetrics();

  // Records UMA metrics if this form submission happened as part of an ablation
  // study or the corresponding control group. This is not virtual because it is
  // called in the destructor.
  void RecordAblationMetrics();

  // Constructor parameters.
  std::string form_type_name_;
  bool is_in_main_frame_;

  // State variables.
  size_t server_record_type_count_ = 0;
  size_t local_record_type_count_ = 0;
  bool has_parsed_form_ = false;
  bool has_logged_interacted_ = false;
  bool has_logged_popup_suppressed_ = false;
  bool has_logged_user_hide_suggestions_ = false;
  bool has_logged_suggestions_shown_ = false;
  bool has_logged_suggestion_filled_ = false;
  bool has_logged_autocomplete_off_ = false;
  bool has_logged_will_submit_ = false;
  bool has_logged_submitted_ = false;
  bool logged_suggestion_filled_was_server_data_ = false;
  bool has_logged_typed_into_non_filled_field_ = false;
  bool has_logged_edited_autofilled_field_ = false;
  AblationGroup ablation_group_ = AblationGroup::kDefault;
  AblationGroup conditional_ablation_group_ = AblationGroup::kDefault;
  absl::optional<base::TimeDelta> time_from_interaction_to_submission_;

  // The last field that was polled for suggestions.
  FormFieldData last_polled_field_;

  // Weak reference.
  raw_ptr<AutofillMetrics::FormInteractionsUkmLogger>
      form_interactions_ukm_logger_;

  // Weak reference.
  const raw_ptr<LogManager> log_manager_;

  AutofillSyncSigninState sync_state_ = AutofillSyncSigninState::kNumSyncStates;
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
