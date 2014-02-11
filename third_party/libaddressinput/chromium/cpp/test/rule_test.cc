// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rule.h"

#include <libaddressinput/address_field.h>

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"

namespace {

using i18n::addressinput::AddressField;
using i18n::addressinput::ADMIN_AREA;
using i18n::addressinput::COUNTRY;
using i18n::addressinput::FormatElement;
using i18n::addressinput::LOCALITY;
using i18n::addressinput::ORGANIZATION;
using i18n::addressinput::POSTAL_CODE;
using i18n::addressinput::RECIPIENT;
using i18n::addressinput::RegionDataConstants;
using i18n::addressinput::Rule;
using i18n::addressinput::STREET_ADDRESS;

bool IsFormatEmpty(const std::vector<std::vector<FormatElement> >& format) {
  for (std::vector<std::vector<FormatElement> >::const_iterator
           it = format.begin();
       it != format.end();
       ++it) {
    if (!it->empty()) {
      return false;
    }
  }
  return true;
}

TEST(RuleTest, CopyOverwritesRule) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule(
      "{"
      "\"fmt\":\"%S%Z\","
      "\"require\":\"SZ\","
      "\"state_name_type\":\"area\","
      "\"zip_name_type\":\"postal\","
      "\"sub_keys\":\"CA~NY~TX\","
      "\"lang\":\"en\","
      "\"languages\":\"en~fr\","
      "\"zip\":\"\\\\d{5}([ \\\\-]\\\\d{4})?\""
      "}"));

  Rule copy;
  EXPECT_NE(rule.GetFormat(), copy.GetFormat());
  EXPECT_NE(rule.GetRequired(), copy.GetRequired());
  EXPECT_NE(rule.GetSubKeys(), copy.GetSubKeys());
  EXPECT_NE(rule.GetLanguages(), copy.GetLanguages());
  EXPECT_NE(rule.GetLanguage(), copy.GetLanguage());
  EXPECT_NE(rule.GetPostalCodeFormat(), copy.GetPostalCodeFormat());
  EXPECT_NE(rule.GetAdminAreaNameMessageId(),
            copy.GetAdminAreaNameMessageId());
  EXPECT_NE(rule.GetInvalidAdminAreaMessageId(),
            copy.GetInvalidAdminAreaMessageId());
  EXPECT_NE(rule.GetPostalCodeNameMessageId(),
            copy.GetPostalCodeNameMessageId());
  EXPECT_NE(rule.GetInvalidPostalCodeMessageId(),
            copy.GetInvalidPostalCodeMessageId());

  copy.CopyFrom(rule);
  EXPECT_EQ(rule.GetFormat(), copy.GetFormat());
  EXPECT_EQ(rule.GetRequired(), copy.GetRequired());
  EXPECT_EQ(rule.GetSubKeys(), copy.GetSubKeys());
  EXPECT_EQ(rule.GetLanguages(), copy.GetLanguages());
  EXPECT_EQ(rule.GetLanguage(), copy.GetLanguage());
  EXPECT_EQ(rule.GetPostalCodeFormat(), copy.GetPostalCodeFormat());
  EXPECT_EQ(rule.GetAdminAreaNameMessageId(),
            copy.GetAdminAreaNameMessageId());
  EXPECT_EQ(rule.GetInvalidAdminAreaMessageId(),
            copy.GetInvalidAdminAreaMessageId());
  EXPECT_EQ(rule.GetPostalCodeNameMessageId(),
            copy.GetPostalCodeNameMessageId());
  EXPECT_EQ(rule.GetInvalidPostalCodeMessageId(),
            copy.GetInvalidPostalCodeMessageId());
}

TEST(RuleTest, ParseOverwritesRule) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule(
      "{"
      "\"fmt\":\"%S%Z\","
      "\"require\":\"SZ\","
      "\"state_name_type\":\"area\","
      "\"zip_name_type\":\"postal\","
      "\"sub_keys\":\"CA~NY~TX\","
      "\"lang\":\"en\","
      "\"languages\":\"en~fr\","
      "\"zip\":\"\\\\d{5}([ \\\\-]\\\\d{4})?\""
      "}"));
  EXPECT_FALSE(IsFormatEmpty(rule.GetFormat()));
  EXPECT_FALSE(rule.GetRequired().empty());
  EXPECT_FALSE(rule.GetSubKeys().empty());
  EXPECT_FALSE(rule.GetLanguages().empty());
  EXPECT_FALSE(rule.GetLanguage().empty());
  EXPECT_FALSE(rule.GetPostalCodeFormat().empty());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_AREA,
            rule.GetAdminAreaNameMessageId());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_INVALID_AREA,
            rule.GetInvalidAdminAreaMessageId());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_POSTAL_CODE_LABEL,
            rule.GetPostalCodeNameMessageId());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_INVALID_POSTAL_CODE_LABEL,
            rule.GetInvalidPostalCodeMessageId());

  ASSERT_TRUE(rule.ParseSerializedRule(
      "{"
      "\"fmt\":\"\","
      "\"require\":\"\","
      "\"state_name_type\":\"do_si\","
      "\"zip_name_type\":\"zip\","
      "\"sub_keys\":\"\","
      "\"lang\":\"\","
      "\"languages\":\"\","
      "\"zip\":\"\""
      "}"));
  EXPECT_TRUE(IsFormatEmpty(rule.GetFormat()));
  EXPECT_TRUE(rule.GetRequired().empty());
  EXPECT_TRUE(rule.GetSubKeys().empty());
  EXPECT_TRUE(rule.GetLanguages().empty());
  EXPECT_TRUE(rule.GetLanguage().empty());
  EXPECT_TRUE(rule.GetPostalCodeFormat().empty());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_DO_SI,
            rule.GetAdminAreaNameMessageId());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_INVALID_DO_SI,
            rule.GetInvalidAdminAreaMessageId());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_ZIP_CODE_LABEL,
            rule.GetPostalCodeNameMessageId());
  EXPECT_EQ(IDS_LIBADDRESSINPUT_I18N_INVALID_ZIP_CODE_LABEL,
            rule.GetInvalidPostalCodeMessageId());
}

TEST(RuleTest, ParseEmptyDataDoesNotOverwriteRule) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule(
      "{"
      "\"fmt\":\"%S%Z\","
      "\"require\":\"SZ\","
      "\"state_name_type\":\"area\","
      "\"zip_name_type\":\"postal\","
      "\"sub_keys\":\"CA~NY~TX\","
      "\"lang\":\"en\","
      "\"languages\":\"en~fr\","
      "\"zip\":\"\\\\d{5}([ \\\\-]\\\\d{4})?\""
      "}"));

  Rule copy;
  copy.CopyFrom(rule);
  ASSERT_TRUE(copy.ParseSerializedRule("{}"));

  EXPECT_EQ(rule.GetFormat(), copy.GetFormat());
  EXPECT_EQ(rule.GetRequired(), copy.GetRequired());
  EXPECT_EQ(rule.GetSubKeys(), copy.GetSubKeys());
  EXPECT_EQ(rule.GetLanguages(), copy.GetLanguages());
  EXPECT_EQ(rule.GetLanguage(), copy.GetLanguage());
  EXPECT_EQ(rule.GetPostalCodeFormat(), copy.GetPostalCodeFormat());
  EXPECT_EQ(rule.GetAdminAreaNameMessageId(),
            copy.GetAdminAreaNameMessageId());
  EXPECT_EQ(rule.GetInvalidAdminAreaMessageId(),
            copy.GetInvalidAdminAreaMessageId());
  EXPECT_EQ(rule.GetPostalCodeNameMessageId(),
            copy.GetPostalCodeNameMessageId());
  EXPECT_EQ(rule.GetInvalidPostalCodeMessageId(),
            copy.GetInvalidPostalCodeMessageId());
}

TEST(RuleTest, ParseFormatWithNewLines) {
  Rule rule;
  ASSERT_TRUE(
      rule.ParseSerializedRule("{\"fmt\":\"%O%n%N%n%A%nAX-%Z %C%nÅLAND\"}"));

  std::vector<std::vector<FormatElement> > expected_format;
  expected_format.push_back(
      std::vector<FormatElement>(1, FormatElement(ORGANIZATION)));
  expected_format.push_back(
      std::vector<FormatElement>(1, FormatElement(RECIPIENT)));
  expected_format.push_back(
      std::vector<FormatElement>(1, FormatElement(STREET_ADDRESS)));
  expected_format.push_back(
      std::vector<FormatElement>(1, FormatElement("AX-")));
  expected_format.back().push_back(FormatElement(POSTAL_CODE));
  expected_format.back().push_back(FormatElement(" "));
  expected_format.back().push_back(FormatElement(LOCALITY));
  expected_format.push_back(
      std::vector<FormatElement>(1, FormatElement("ÅLAND")));

  EXPECT_EQ(expected_format, rule.GetFormat());
}

TEST(RuleTest, DoubleTokenPrefixDoesNotCrash) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"%%R\"}"));
}

TEST(RuleTest, DoubleNewlineFormatDoesNotCrash) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"%n%n\"}"));
}

TEST(RuleTest, FormatTokenWithoutPrefixDoesNotCrash) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"R\"}"));
}

TEST(RuleTest, ParseDuplicateTokenInFormatDoesNotCrash) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"%R%R\"}"));
}

TEST(RuleTest, ParseInvalidFormatFieldsDoesNotCrash) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"%K%L\"}"));
}

TEST(RuleTest, PrefixWithoutTokenFormatDoesNotCrash) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"%\"}"));
}

TEST(RuleTest, EmptyStringFormatDoesNotCrash) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{\"fmt\":\"\"}"));
}

TEST(RuleTest, ParseRequiredFields) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"require\":\"ONAZC\"}"));
  std::vector<AddressField> expected;
  expected.push_back(ORGANIZATION);
  expected.push_back(RECIPIENT);
  expected.push_back(STREET_ADDRESS);
  expected.push_back(POSTAL_CODE);
  expected.push_back(LOCALITY);
  EXPECT_EQ(expected, rule.GetRequired());
}

TEST(RuleTest, ParseEmptyStringRequiredFields) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"require\":\"\"}"));
  EXPECT_TRUE(rule.GetRequired().empty());
}

TEST(RuleTest, ParseInvalidRequiredFields) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"require\":\"garbage\"}"));
  EXPECT_TRUE(rule.GetRequired().empty());
}

TEST(RuleTest, ParseDuplicateRequiredFields) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"require\":\"SSS\"}"));
  EXPECT_EQ(std::vector<AddressField>(3, ADMIN_AREA), rule.GetRequired());
}

TEST(RuleTest, ParsesSubKeysCorrectly) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"sub_keys\":\"CA~NY~TX\"}"));
  std::vector<std::string> expected;
  expected.push_back("CA");
  expected.push_back("NY");
  expected.push_back("TX");
  EXPECT_EQ(expected, rule.GetSubKeys());
}

TEST(RuleTest, ParsesLanguageCorrectly) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"lang\":\"en\"}"));
  EXPECT_EQ("en", rule.GetLanguage());
}

TEST(RuleTest, ParsesLanguagesCorrectly) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule("{\"languages\":\"de~fr~it\"}"));
  std::vector<std::string> expected;
  expected.push_back("de");
  expected.push_back("fr");
  expected.push_back("it");
  EXPECT_EQ(expected, rule.GetLanguages());
}

TEST(RuleTest, ParsesPostalCodeFormatCorrectly) {
  Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule(
      "{"
      "\"zip\":\"\\\\d{5}([ \\\\-]\\\\d{4})?\""
      "}"));
  EXPECT_EQ("\\d{5}([ \\-]\\d{4})?", rule.GetPostalCodeFormat());
}

TEST(RuleTest, EmptyStringIsNotValid) {
  Rule rule;
  EXPECT_FALSE(rule.ParseSerializedRule(std::string()));
}

TEST(RuleTest, EmptyDictionaryIsValid) {
  Rule rule;
  EXPECT_TRUE(rule.ParseSerializedRule("{}"));
}

TEST(RuleTest, ParseSubKeyTest) {
  i18n::addressinput::Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule(
      "{ \"sub_keys\": \"FOO~BAR~BAZ\","
      "  \"sub_names\": \"Foolandia~Bartopolis~Bazmonia\","
      "  \"sub_lnames\": \"Foolandia2~Bartopolis2~Bazmonia2\" }"));
  EXPECT_EQ(3U, rule.GetSubKeys().size());

  std::string sub_key;
  EXPECT_TRUE(rule.CanonicalizeSubKey("BAR", &sub_key));
  EXPECT_EQ("BAR", sub_key);
  sub_key.clear();

  EXPECT_TRUE(rule.CanonicalizeSubKey("Bartopolis", &sub_key));
  EXPECT_EQ("BAR", sub_key);
  sub_key.clear();

  EXPECT_TRUE(rule.CanonicalizeSubKey("Bartopolis2", &sub_key));
  EXPECT_EQ("BAR", sub_key);
  sub_key.clear();

  EXPECT_FALSE(rule.CanonicalizeSubKey("Beertopia", &sub_key));
  EXPECT_EQ("", sub_key);
}

struct LabelData {
  LabelData(const std::string& data, int name_id, int error_id)
      : data(data), name_id(name_id), error_id(error_id) {}

  ~LabelData() {}

  std::string data;
  int name_id;
  int error_id;
};

// Tests for parsing the postal code name.
class PostalCodeNameParseTest : public testing::TestWithParam<LabelData> {
 protected:
  Rule rule_;
};

// Verifies that a postal code name is parsed correctly.
TEST_P(PostalCodeNameParseTest, ParsedCorrectly) {
  ASSERT_TRUE(rule_.ParseSerializedRule(GetParam().data));
  EXPECT_EQ(GetParam().name_id, rule_.GetPostalCodeNameMessageId());
  EXPECT_EQ(GetParam().error_id, rule_.GetInvalidPostalCodeMessageId());
  EXPECT_EQ(GetParam().error_id, rule_.GetInvalidFieldMessageId(POSTAL_CODE));
}

// Test parsing all postal code names.
INSTANTIATE_TEST_CASE_P(
    AllPostalCodeNames, PostalCodeNameParseTest,
    testing::Values(
        LabelData("{\"zip_name_type\":\"postal\"}",
                  IDS_LIBADDRESSINPUT_I18N_POSTAL_CODE_LABEL,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_POSTAL_CODE_LABEL),
        LabelData("{\"zip_name_type\":\"zip\"}",
                  IDS_LIBADDRESSINPUT_I18N_ZIP_CODE_LABEL,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_ZIP_CODE_LABEL)));

// Tests for parsing the administrative area name.
class AdminAreaNameParseTest : public testing::TestWithParam<LabelData> {
 protected:
  Rule rule_;
};

// Verifies that an administrative area name is parsed correctly.
TEST_P(AdminAreaNameParseTest, ParsedCorrectly) {
  ASSERT_TRUE(rule_.ParseSerializedRule(GetParam().data));
  EXPECT_EQ(GetParam().name_id, rule_.GetAdminAreaNameMessageId());
  EXPECT_EQ(GetParam().error_id, rule_.GetInvalidAdminAreaMessageId());
  EXPECT_EQ(GetParam().error_id, rule_.GetInvalidFieldMessageId(ADMIN_AREA));
}

// Test parsing all administrative area names.
INSTANTIATE_TEST_CASE_P(
    AllAdminAreaNames, AdminAreaNameParseTest,
    testing::Values(
        LabelData("{\"state_name_type\":\"area\"}",
                  IDS_LIBADDRESSINPUT_I18N_AREA,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_AREA),
        LabelData("{\"state_name_type\":\"county\"}",
                  IDS_LIBADDRESSINPUT_I18N_COUNTY_LABEL,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_COUNTY_LABEL),
        LabelData("{\"state_name_type\":\"department\"}",
                  IDS_LIBADDRESSINPUT_I18N_DEPARTMENT,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_DEPARTMENT),
        LabelData("{\"state_name_type\":\"district\"}",
                  IDS_LIBADDRESSINPUT_I18N_DEPENDENT_LOCALITY_LABEL,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_DEPENDENT_LOCALITY_LABEL),
        LabelData("{\"state_name_type\":\"do_si\"}",
                  IDS_LIBADDRESSINPUT_I18N_DO_SI,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_DO_SI),
        LabelData("{\"state_name_type\":\"emirate\"}",
                  IDS_LIBADDRESSINPUT_I18N_EMIRATE,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_EMIRATE),
        LabelData("{\"state_name_type\":\"island\"}",
                  IDS_LIBADDRESSINPUT_I18N_ISLAND,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_ISLAND),
        LabelData("{\"state_name_type\":\"parish\"}",
                  IDS_LIBADDRESSINPUT_I18N_PARISH,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_PARISH),
        LabelData("{\"state_name_type\":\"prefecture\"}",
                  IDS_LIBADDRESSINPUT_I18N_PREFECTURE,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_PREFECTURE),
        LabelData("{\"state_name_type\":\"province\"}",
                  IDS_LIBADDRESSINPUT_I18N_PROVINCE,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_PROVINCE),
        LabelData("{\"state_name_type\":\"state\"}",
                  IDS_LIBADDRESSINPUT_I18N_STATE_LABEL,
                  IDS_LIBADDRESSINPUT_I18N_INVALID_STATE_LABEL)));

// Tests for rule parsing.
class RuleParseTest : public testing::TestWithParam<std::string> {
 protected:
  const std::string& GetData() const {
    return RegionDataConstants::GetRegionData(GetParam());
  }

  Rule rule_;
};

// Verifies that an address format does not contain consecutive lines with
// multiple fields each. Such address format (e.g. {{ELEMENT, ELEMENT},
// {ELEMENT, ELEMENT}}) will result in incorrect behavior of BuildComponents()
// public API.
TEST_P(RuleParseTest, ConsecutiveLinesWithMultipleFields) {
  ASSERT_TRUE(rule_.ParseSerializedRule(GetData()));
  bool previous_line_has_single_field = true;
  for (std::vector<std::vector<FormatElement> >::const_iterator
           line_it = rule_.GetFormat().begin();
       line_it != rule_.GetFormat().end();
       ++line_it) {
    int num_fields = 0;
    for (std::vector<FormatElement>::const_iterator
             element_it = line_it->begin();
         element_it != line_it->end();
         ++element_it) {
      if (element_it->IsField()) {
        ++num_fields;
      }
    }
    if (num_fields == 0) {
      continue;
    }
    ASSERT_TRUE(num_fields == 1 || previous_line_has_single_field)
        << GetParam() << ": " << GetData();
    previous_line_has_single_field = num_fields == 1;
  }
}

// Verifies that a street line is surrounded by either newlines or spaces. A
// different format will result in incorrect behavior in
// AddressData::BuildDisplayLines().
TEST_P(RuleParseTest, StreetAddressSurroundingElements) {
  ASSERT_TRUE(rule_.ParseSerializedRule(GetData()));
  for (std::vector<std::vector<FormatElement> >::const_iterator
           line_it = rule_.GetFormat().begin();
       line_it != rule_.GetFormat().end();
       ++line_it) {
    for (size_t i = 0; i < line_it->size(); ++i) {
      const FormatElement& element = line_it->at(i);
      if (element.IsField() && element.field == STREET_ADDRESS) {
        bool surrounded_by_newlines = line_it->size() == 1;
        bool surrounded_by_spaces =
            i > 0 &&
            i < line_it->size() - 1 &&
            !line_it->at(i - 1).IsField() &&
            line_it->at(i - 1).literal == " " &&
            !line_it->at(i + 1).IsField() &&
            line_it->at(i + 1).literal == " ";
        EXPECT_TRUE(surrounded_by_newlines || surrounded_by_spaces)
            << GetParam() << ": " << GetData();
      }
    }
  }
}

// Test parsing all region data.
INSTANTIATE_TEST_CASE_P(
    AllRulesTest, RuleParseTest,
    testing::ValuesIn(RegionDataConstants::GetRegionCodes()));

}  // namespace
