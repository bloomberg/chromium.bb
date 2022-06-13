// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutocompleteHistoryManager : public AutocompleteHistoryManager {
 public:
  MockAutocompleteHistoryManager();
  ~MockAutocompleteHistoryManager();

  MOCK_METHOD(
      void,
      OnGetSingleFieldSuggestions,
      (int query_id,
       bool is_autocomplete_enabled,
       bool autoselect_first_suggestion,
       const std::u16string& name,
       const std::u16string& prefix,
       const std::string& form_control_type,
       base::WeakPtr<AutocompleteHistoryManager::SuggestionsHandler> handler),
      (override));
  MOCK_METHOD(void,
              OnWillSubmitForm,
              (const FormData& form, bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(void,
              OnWebDataServiceRequestDone,
              (WebDataServiceBase::Handle, std::unique_ptr<WDTypedResult>),
              (override));
  MOCK_METHOD(void,
              CancelPendingQueries,
              (const AutocompleteHistoryManager::SuggestionsHandler*),
              (override));
  MOCK_METHOD(void,
              OnRemoveCurrentSingleFieldSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              OnSingleFieldSuggestionSelected,
              (const std::u16string&),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOCOMPLETE_HISTORY_MANAGER_H_
