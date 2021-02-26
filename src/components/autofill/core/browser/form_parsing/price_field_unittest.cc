// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/price_field.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/pattern_provider/test_pattern_provider.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class PriceFieldTest : public testing::Test {
 public:
  PriceFieldTest() = default;
  PriceFieldTest(const PriceFieldTest&) = delete;
  PriceFieldTest& operator=(const PriceFieldTest&) = delete;

 protected:
  // Downcast for tests.
  static std::unique_ptr<PriceField> Parse(AutofillScanner* scanner) {
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    std::unique_ptr<FormField> field =
        PriceField::Parse(scanner, /*page_language=*/"", nullptr);
    return std::unique_ptr<PriceField>(
        static_cast<PriceField*>(field.release()));
  }

  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<PriceField> field_;
  FieldCandidatesMap field_candidates_map_;

  // RAII object to mock the the PatternProvider.
  TestPatternProvider test_pattern_provider_;
};

TEST_F(PriceFieldTest, ParsePrice) {
  FormFieldData price_field;
  price_field.form_control_type = "text";

  price_field.label = ASCIIToUTF16("name your price");
  price_field.name = ASCIIToUTF16("userPrice");

  list_.push_back(
      std::make_unique<AutofillField>(price_field, ASCIIToUTF16("price1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_NE(nullptr, field_.get());
  field_->AddClassifications(&field_candidates_map_);
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("price1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(PRICE,
            field_candidates_map_[ASCIIToUTF16("price1")].BestHeuristicType());
}

TEST_F(PriceFieldTest, ParseNonPrice) {
  FormFieldData address_field;
  address_field.form_control_type = "text";

  address_field.label = ASCIIToUTF16("Name");
  address_field.name = ASCIIToUTF16("firstName");

  list_.push_back(
      std::make_unique<AutofillField>(address_field, ASCIIToUTF16("name1")));

  AutofillScanner scanner(list_);
  field_ = Parse(&scanner);
  ASSERT_EQ(nullptr, field_.get());
}

}  // namespace autofill
