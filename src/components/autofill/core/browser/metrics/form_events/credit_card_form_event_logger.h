// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

enum class UnmaskAuthFlowType;

class CreditCardFormEventLogger : public FormEventLoggerBase {
 public:
  enum class UnmaskAuthFlowEvent {
    // Authentication prompt is shown.
    kPromptShown = 0,
    // Authentication prompt successfully completed.
    kPromptCompleted = 1,
    // Form was submitted.
    kFormSubmitted = 2,
    kMaxValue = kFormSubmitted,
  };

  CreditCardFormEventLogger(
      bool is_in_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      PersonalDataManager* personal_data_manager,
      AutofillClient* client);

  ~CreditCardFormEventLogger() override;

  void set_is_context_secure(bool is_context_secure) {
    is_context_secure_ = is_context_secure;
  }

  void set_suggestions(std::vector<Suggestion> suggestions);

  void OnDidShowSuggestions(const FormStructure& form,
                            const AutofillField& field,
                            const base::TimeTicks& form_parsed_timestamp,
                            AutofillSyncSigninState sync_state,
                            bool off_the_record) override;

  void OnDidSelectCardSuggestion(const CreditCard& credit_card,
                                 const FormStructure& form,
                                 AutofillSyncSigninState sync_state);

  // In case of masked cards, caller must make sure this gets called before
  // the card is upgraded to a full card.
  void OnDidFillSuggestion(const CreditCard& credit_card,
                           const FormStructure& form,
                           const AutofillField& field,
                           const base::flat_map<FieldGlobalId, ServerFieldType>&
                               field_types_to_be_filled_before_security_policy,
                           const base::flat_map<FieldGlobalId, ServerFieldType>&
                               field_types_filled_after_security_policy,
                           AutofillSyncSigninState sync_state);

  // Logging what type of authentication flow was prompted.
  void LogCardUnmaskAuthenticationPromptShown(UnmaskAuthFlowType flow);

  // Logging when an authentication prompt is completed.
  void LogCardUnmaskAuthenticationPromptCompleted(UnmaskAuthFlowType flow);

  // Allows mocking that a virtual card was selected, for unit tests that don't
  // run the actual Autofill suggestions dropdown UI.
  void set_latest_selected_card_was_virtual_card_for_testing(
      bool latest_selected_card_was_virtual_card) {
    latest_selected_card_was_virtual_card_ =
        latest_selected_card_was_virtual_card;
  }

 protected:
  // FormEventLoggerBase pure-virtual overrides.
  void RecordPollSuggestions() override;
  void RecordParseForm() override;
  void RecordShowSuggestions() override;

  // FormEventLoggerBase virtual overrides.
  void LogWillSubmitForm(const FormStructure& form) override;
  void LogFormSubmitted(const FormStructure& form) override;
  void LogUkmInteractedWithForm(FormSignature form_signature) override;
  void OnSuggestionsShownOnce(const FormStructure& form) override;
  void OnSuggestionsShownSubmittedOnce(const FormStructure& form) override;
  void OnLog(const std::string& name,
             FormEvent event,
             const FormStructure& form) const override;

  // Bringing base class' Log function into scope to allow overloading.
  using FormEventLoggerBase::Log;

 private:
  FormEvent GetCardNumberStatusFormEvent(const CreditCard& credit_card);
  void RecordCardUnmaskFlowEvent(UnmaskAuthFlowType flow,
                                 UnmaskAuthFlowEvent event);
  bool DoesCardHaveOffer(const CreditCard& credit_card);
  // Returns whether the shown suggestions included a virtual credit card.
  bool DoSuggestionsIncludeVirtualCard();

  bool is_context_secure_ = false;
  UnmaskAuthFlowType current_authentication_flow_;
  bool has_logged_masked_server_card_suggestion_selected_ = false;
  bool has_logged_virtual_card_suggestion_selected_ = false;
  bool logged_suggestion_filled_was_masked_server_card_ = false;
  bool logged_suggestion_filled_was_virtual_card_ = false;
  // If true, the most recent card to be selected as an Autofill suggestion was
  // a virtual card. False for all other card types.
  bool latest_selected_card_was_virtual_card_ = false;
  std::vector<Suggestion> suggestions_;
  bool has_eligible_offer_ = false;
  bool card_selected_has_offer_ = false;

  // Weak references.
  raw_ptr<PersonalDataManager> personal_data_manager_;
  raw_ptr<AutofillClient> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_H_
