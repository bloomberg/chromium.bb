// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_

#include <vector>
#include "base/callback.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/website_login_manager.h"

namespace autofill_assistant {
namespace user_data {

// Validate the completeness of a contact.
std::vector<std::string> GetContactValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options);

// Sorts the given contacts based on completeness, and returns a vector of
// indices in sorted order. Full contacts will be ordered before empty ones,
// and for equally complete contacts, this falls back to sorting based on last
// used.
std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<Contact>>& contacts);

// Get the default selection for the current list of contacts. Returns -1 if no
// default selection is possible.
int GetDefaultContact(const CollectUserDataOptions& collect_user_data_options,
                      const std::vector<std::unique_ptr<Contact>>& contacts);

// Validate the completeness of a shipping address.
std::vector<std::string> GetShippingAddressValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options);

// Sorts the given addresses based on completeness, and returns a vector of
// indices in sorted order. Full addresses will be ordered before empty ones,
// and for equally complete profiles, this falls back to sorting based on
// last used.
std::vector<int> SortShippingAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<Address>>& addresses);

// Get the default selection for the current list of addresses. Returns -1 if no
// no default selection is possible.
int GetDefaultShippingAddress(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<Address>>& addresses);

std::vector<std::string> GetPaymentInstrumentValidationErrors(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_address,
    const CollectUserDataOptions& collect_user_data_options);

// Sorts the given payment instruments by completeness, and returns a vector
// of payment instrument indices in sorted order. Full payment instruments will
// be ordered before empty ones, and for equally complete payment instruments,
// this falls back to sorting based on the full name on the credit card.
std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

// Get the default selection for the current list of payment instruments.
// Returns -1 if no default selection is possible.
int GetDefaultPaymentInstrument(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile);

// Compare contact fields only. This comparison checks a subset of
// AutofillProfile::Compare. Falls back to comparing the GUIDs if nothing else
// is to be compared.
bool CompareContactDetails(
    const CollectUserDataOptions& collect_user_data_options,
    const autofill::AutofillProfile* a,
    const autofill::AutofillProfile* b);

// Get a formatted client value. The replacement is treated as strict,
// meaning a missing value will lead to a failed ClientStatus.
// This method returns:
// - INVALID_ACTION, if the value is empty.
// - INVALID_ACTION, if a profile is provided and it is empty.
// - PRECONDITION_FAILED, if the requested profile is not found.
// - AUTOFILL_INFO_NOT_AVAILABLE, if a key from  an AUtofill source cannot be
//   resolved.
// - CLIENT_MEMORY_KEY_NOT_AVAILABLE, if a key from the client memory cannot be
//   resolved.
// - EMPTY_VALUE_EXPRESSION_RESULT, if the result is an empty string.
// - ACTION_APPLIED otherwise.
ClientStatus GetFormattedClientValue(const AutofillValue& autofill_value,
                                     const UserData& user_data,
                                     std::string* out_value);
ClientStatus GetFormattedClientValue(
    const AutofillValueRegexp& autofill_value_regexp,
    const UserData& user_data,
    std::string* out_value);

// Get a password manager value from the |UserData|. Returns the user name
// directly and resolves the password from the |WebsiteLoginManager|. If the
// login credentials do not exist, fails with |PRECONDITION_FAILED|. If the
// origin of the |target_element| does not match the origin of the login
// credentials, fails with |PASSWORD_ORIGIN_MISMATCH|.
void GetPasswordManagerValue(
    const PasswordManagerValue& password_manager_value,
    const ElementFinder::Result& target_element,
    const UserData* user_data,
    WebsiteLoginManager* website_login_manager,
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback);

// Retrieve a single string value stored in |UserData| under
// |client_memory_key|. If the value is not present or not a single string,
// fails with |PRECONDITION_FAILED|.
ClientStatus GetClientMemoryStringValue(const std::string& client_memory_key,
                                        const UserData* user_data,
                                        std::string* out_value);

// Take a |text_value| and resolve its content to a string. Reports the result
// through the |callback|.
void ResolveTextValue(
    const TextValue& text_value,
    const ElementFinder::Result& target_element,
    const ActionDelegate* action_delegate,
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback);

// Returns the next selection state for when an event of type |event_type|
// happens while the current state is |old_state|.
Metrics::UserDataSelectionState GetNewSelectionState(
    Metrics::UserDataSelectionState old_state,
    UserDataEventType event_type);

// Returns the bit array describing which fields are present in |profile|, using
// Metrics::AutofillAssistantProfileFields as columns.
// If |profile| is nullptr, returns zero (i.e. all fields are considered
// missing).
int GetFieldBitArrayForAddress(const autofill::AutofillProfile* profile);

// Returns the bit array describing which fields are present in |card|, using
// Metrics::AutofillAssistantCreditCardFields as columns.
// If |card| is nullptr, returns zero (i.e. all fields are considered
// missing).
int GetFieldBitArrayForCreditCard(const autofill::CreditCard* card);

}  // namespace user_data
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
