// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTION_SELECTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTION_SELECTION_H_

#include <vector>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestion.h"

namespace autofill {

class AutofillProfile;
class AutofillProfileComparator;
class AutofillType;

namespace suggestion_selection {

extern const size_t kMaxSuggestedProfilesCount;
extern const size_t kMaxUniqueSuggestionsCount;

// Matches based on prefix search, and limits number of profiles.
// Returns the top matching suggestions based on prefix search, and adds the
// corresponding profiles to |matched_profiles|. At most
// |kMaxSuggestedProfilesCount| are returned.
std::vector<Suggestion> GetPrefixMatchedSuggestions(
    const AutofillType& type,
    const base::string16& field_contents_canon,
    const AutofillProfileComparator& comparator,
    const std::vector<AutofillProfile*>& profiles,
    std::vector<AutofillProfile*>* matched_profiles);

// Dedupes given suggestions based on if one is a subset of the other.
// Returns unique_suggestions, and adds the corresponding profiles to
// |unique_matched_profiles|. At most |kMaxUniqueSuggestionsCount| are returned.
std::vector<Suggestion> GetUniqueSuggestions(
    const std::vector<ServerFieldType>& other_field_types,
    const std::string app_locale,
    const std::vector<AutofillProfile*> matched_profiles,
    const std::vector<Suggestion>& suggestions,
    std::vector<AutofillProfile*>* unique_matched_profiles);

// Returns whether the |suggestion_canon| is valid considering the
// |field_contents_canon|, the |type| and |is_masked_server_card|. Assigns true
// to |is_prefix_matched| if the |field_contents_canon| is a prefix to
// |suggestion_canon|, assigns false otherwise.
bool IsValidSuggestionForFieldContents(base::string16 suggestion_canon,
                                       base::string16 field_contents_canon,
                                       const AutofillType& type,
                                       bool is_masked_server_card,
                                       bool* is_prefix_matched);

// Removes profiles that haven't been used after |min_last_used| from
// |profiles|. The relative ordering of |profiles| is maintained.
void RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>* profiles);

}  // namespace suggestion_selection
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTION_SELECTION_H_
