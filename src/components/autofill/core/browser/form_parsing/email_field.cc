// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/email_field.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

namespace autofill {

// static
std::unique_ptr<FormField> EmailField::Parse(AutofillScanner* scanner,
                                             const std::string& page_language,
                                             LogManager* log_manager) {
  AutofillField* field;
  auto& patterns = PatternProvider::GetInstance().GetMatchPatterns(
      "EMAIL_ADDRESS", page_language);
  if (ParseFieldSpecifics(scanner, base::UTF8ToUTF16(kEmailRe),
                          MATCH_DEFAULT | MATCH_EMAIL, patterns, &field,
                          {log_manager, "kEmailRe"})) {
    return std::make_unique<EmailField>(field);
  }

  return nullptr;
}

EmailField::EmailField(const AutofillField* field) : field_(field) {}

void EmailField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  AddClassification(field_, EMAIL_ADDRESS, kBaseEmailParserScore,
                    field_candidates);
}

}  // namespace autofill
