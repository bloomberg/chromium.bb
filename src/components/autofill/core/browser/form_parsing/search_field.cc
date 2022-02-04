// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/search_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

namespace autofill {

// static
std::unique_ptr<FormField> SearchField::Parse(AutofillScanner* scanner,
                                              const LanguageCode& page_language,
                                              LogManager* log_manager) {
  AutofillField* field;
  auto& patterns = PatternProvider::GetInstance().GetMatchPatterns(
      SEARCH_TERM, page_language);

  if (ParseFieldSpecifics(scanner, kSearchTermRe,
                          kDefaultMatchParamsWith<MatchFieldType::kSearch,
                                                  MatchFieldType::kTextArea>,
                          patterns, &field, {log_manager, "kSearchTermRe"})) {
    return std::make_unique<SearchField>(field);
  }

  return nullptr;
}

SearchField::SearchField(const AutofillField* field) : field_(field) {}

void SearchField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, SEARCH_TERM, kBaseSearchParserScore,
                    field_candidates);
}

}  // namespace autofill
