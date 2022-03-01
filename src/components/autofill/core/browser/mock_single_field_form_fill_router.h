// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SINGLE_FIELD_FORM_FILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SINGLE_FIELD_FORM_FILL_ROUTER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockSingleFieldFormFillRouter : public SingleFieldFormFillRouter {
 public:
  explicit MockSingleFieldFormFillRouter(
      AutocompleteHistoryManager* autocomplete_history_manager);
  ~MockSingleFieldFormFillRouter() override;

  MOCK_METHOD(
      void,
      OnGetSingleFieldSuggestions,
      (int query_id,
       bool is_autocomplete_enabled,
       bool autoselect_first_suggestion,
       const std::u16string& name,
       const std::u16string& prefix,
       const std::string& form_control_type,
       base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler),
      (override));
  MOCK_METHOD(void,
              OnWillSubmitForm,
              (const FormData& form, bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(void,
              CancelPendingQueries,
              (const SingleFieldFormFiller::SuggestionsHandler*),
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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_SINGLE_FIELD_FORM_FILL_ROUTER_H_
