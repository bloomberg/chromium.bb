// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/pattern_provider/test_pattern_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::features::kAutofillFixFillableFieldTypes;
using base::ASCIIToUTF16;

namespace autofill {

TEST(FormFieldTest, Match) {
  AutofillField field;

  // Empty strings match.
  EXPECT_TRUE(FormField::Match(&field, base::string16(), MATCH_LABEL));

  // Empty pattern matches non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_TRUE(FormField::Match(&field, base::string16(), MATCH_LABEL));

  // Strictly empty pattern matches empty string.
  field.label = base::string16();
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^$"), MATCH_LABEL));

  // Strictly empty pattern does not match non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^$"), MATCH_LABEL));

  // Non-empty pattern doesn't match empty string.
  field.label = base::string16();
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("a"), MATCH_LABEL));

  // Beginning of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^head"), MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^tail"), MATCH_LABEL));

  // End of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("head$"), MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("tail$"), MATCH_LABEL));

  // Exact.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^head$"), MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^tail$"), MATCH_LABEL));
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("^head_tail$"), MATCH_LABEL));

  // Escaped dots.
  field.label = ASCIIToUTF16("m.i.");
  // Note: This pattern is misleading as the "." characters are wild cards.
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m.i."), MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."), MATCH_LABEL));
  field.label = ASCIIToUTF16("mXiX");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m.i."), MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."), MATCH_LABEL));

  // Repetition.
  field.label = ASCIIToUTF16("headtail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("head.*tail"), MATCH_LABEL));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("head.*tail"), MATCH_LABEL));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("head.*tail"), MATCH_LABEL));
  field.label = ASCIIToUTF16("headtail");
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("head.+tail"), MATCH_LABEL));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("head.+tail"), MATCH_LABEL));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("head.+tail"), MATCH_LABEL));

  // Alternation.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("head|other"), MATCH_LABEL));
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("tail|other"), MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("bad|good"), MATCH_LABEL));

  // Case sensitivity.
  field.label = ASCIIToUTF16("xxxHeAd_tAiLxxx");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head_tail"), MATCH_LABEL));

  // Word boundaries.
  field.label = ASCIIToUTF16("contains word:");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("\\bword\\b"), MATCH_LABEL));
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("\\bcon\\b"), MATCH_LABEL));
  // Make sure the circumflex in 'crepe' is not treated as a word boundary.
  field.label = base::UTF8ToUTF16("cr\xC3\xAApe");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("\\bcr\\b"), MATCH_LABEL));
}

// Test that we ignore checkable elements.
TEST(FormFieldTest, ParseFormFields) {
  TestPatternProvider test_pattern_provider_;

  std::vector<std::unique_ptr<AutofillField>> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  field_data.label = ASCIIToUTF16("Is PO Box");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // Does not parse since there are only field and it's checkable.
  // An empty page_language means the language is unknown and patterns of all
  // languages are used.
  EXPECT_TRUE(
      FormField::ParseFormFields(fields, /*page_language=*/"", true).empty());

  // reset |is_checkable| to false.
  field_data.check_status = FormFieldData::CheckStatus::kNotCheckable;

  field_data.label = ASCIIToUTF16("Address line1");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // Parse a single address line 1 field.
  ASSERT_EQ(
      0u,
      FormField::ParseFormFields(fields, /*page_language=*/"", true).size());

  // Parses address line 1 and 2.
  field_data.label = ASCIIToUTF16("Address line2");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // An empty page_language means the language is unknown and patterns of
  // all languages are used.
  ASSERT_EQ(
      0u,
      FormField::ParseFormFields(fields, /*page_language=*/"", true).size());
}

// Test that the minimum number of required fields for the heuristics considers
// whether a field is actually fillable.
TEST(FormFieldTest, ParseFormFieldEnforceMinFillableFields) {
  TestPatternProvider test_pattern_provider_;

  std::vector<std::unique_ptr<AutofillField>> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.label = ASCIIToUTF16("Address line 1");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  field_data.label = ASCIIToUTF16("Address line 2");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // Don't parse forms with 2 fields.
  // An empty page_language means the language is unknown and patterns of all
  // languages are used.
  EXPECT_EQ(
      0u,
      FormField::ParseFormFields(fields, /*page_language=*/"", true).size());

  field_data.label = ASCIIToUTF16("Search");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // Before the fix in kAutofillFixFillableFieldTypes, we would parse the form
  // now, although a search field is not fillable.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kAutofillFixFillableFieldTypes);
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    EXPECT_EQ(
        3u,
        FormField::ParseFormFields(fields, /*page_language=*/"", true).size());
  }

  // With the fix, we don't parse the form because search fields are not
  // fillable (therefore, the form has only 2 fillable fields).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kAutofillFixFillableFieldTypes);
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    const FieldCandidatesMap field_candidates_map =
        FormField::ParseFormFields(fields, /*page_language=*/"", true);
    EXPECT_EQ(
        0u,
        FormField::ParseFormFields(fields, /*page_language=*/"", true).size());
  }
}

}  // namespace autofill
