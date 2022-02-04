// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

namespace autofill {

// static
std::unique_ptr<FormField> PriceField::Parse(AutofillScanner* scanner,
                                             const LanguageCode& page_language,
                                             LogManager* log_manager) {
  AutofillField* field;
  const std::vector<MatchingPattern>& price_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("PRICE", page_language);

  if (ParseFieldSpecifics(
          scanner, kPriceRe,
          kDefaultMatchParamsWith<
              MatchFieldType::kNumber, MatchFieldType::kSelect,
              MatchFieldType::kTextArea, MatchFieldType::kSearch>,
          price_patterns, &field, {log_manager, "kPriceRe"})) {
    return std::make_unique<PriceField>(field);
  }

  return nullptr;
}

PriceField::PriceField(const AutofillField* field) : field_(field) {}

void PriceField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, PRICE, kBasePriceParserScore, field_candidates);
}

}  // namespace autofill
