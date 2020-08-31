// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Autofill component.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_

#include <stddef.h>  // For size_t

#include "base/time/time.h"

namespace autofill {

// The origin of an AutofillDataModel created or modified in the settings page.
extern const char kSettingsOrigin[];

// The number of fields required by Autofill to execute its heuristic and
// crowd-sourcing query/upload routines.
size_t MinRequiredFieldsForHeuristics();
size_t MinRequiredFieldsForQuery();
size_t MinRequiredFieldsForUpload();

// The maximum number of form fields we are willing to parse, due to
// computational costs.  Several examples of forms with lots of fields that are
// not relevant to Autofill: (1) the Netflix queue; (2) the Amazon wishlist;
// (3) router configuration pages; and (4) other configuration pages, e.g. for
// Google code project settings.
// Copied to components/autofill/ios/form_util/resources/fill.js.
const size_t kMaxParseableFields = 200;

// The maximum number of allowed calls to CreditCard::GetMatchingTypes() and
// AutofillProfile::GetMatchingTypeAndValidities().
// If #fields * (#profiles + #credit-cards) exceeds this number, type matching
// and voting is omitted.
// The rationale is that for a form with |kMaxParseableFields| = 200 fields,
// this still allows for 25 profiles plus credit cars.
const size_t kMaxTypeMatchingCalls = 5000;

// The minimum number of fields in a form that contains only password fields to
// upload the form to and request predictions from the Autofill servers.
const size_t kRequiredFieldsForFormsWithOnlyPasswordFields = 2;

// Special query id used between the browser and the renderer when the action
// is initiated from the browser.
const int kNoQueryId = -1;

// Options bitmask values for AutofillHostMsg_ShowPasswordSuggestions IPC
enum ShowPasswordSuggestionsOptions {
  SHOW_ALL = 1 << 0 /* show all credentials, not just ones matching username */,
  IS_PASSWORD_FIELD = 1 << 1 /* input field is a password field */
};

// Constants for the soft/hard deletion of Autofill data.
constexpr base::TimeDelta kDisusedDataModelTimeDelta =
    base::TimeDelta::FromDays(180);
constexpr base::TimeDelta kDisusedDataModelDeletionTimeDelta =
    base::TimeDelta::FromDays(395);

// Returns if the entry with the given |use_date| is deletable? (i.e. has not
// been used for a long time).
bool IsAutofillEntryWithUseDateDeletable(const base::Time& use_date);

// The period after which autocomplete entries should be cleaned-up in days.
// Equivalent to roughly 14 months.
const int64_t kAutocompleteRetentionPolicyPeriodInDays = 14 * 31;

// Limits the number of times the value of a specific type can be filled into a
// form.
constexpr int kTypeValueFormFillingLimit = 9;

// Credit card numbers are sometimes distributed between up to 19 individual
// fields. Therefore, credit cards need a higher limit compared to
// |kTypeValueFormFillingLimit|.
constexpr int kCreditCardTypeValueFormFillingLimit = 19;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
