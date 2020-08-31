// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/definition_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;

}

class DefinitionResultParserTest : public testing::Test {
 public:
  DefinitionResultParserTest()
      : parser_(std::make_unique<DefinitionResultParser>()) {}

  DefinitionResultParserTest(const DefinitionResultParserTest&) = delete;
  DefinitionResultParserTest& operator=(const DefinitionResultParserTest&) =
      delete;

 protected:
  Value BuildDictionaryResult(const std::string& query_term,
                              const std::string& phonetic_str,
                              const std::string& definition) {
    Value result(Value::Type::DICTIONARY);

    if (!query_term.empty())
      result.SetStringPath("dictionaryResult.queryTerm", query_term);

    // Build definition entry.
    Value entries(Value::Type::LIST);
    Value entry(Value::Type::DICTIONARY);

    // Build phonetics.
    if (!phonetic_str.empty()) {
      Value phonetics(Value::Type::LIST);
      Value phonetic(Value::Type::DICTIONARY);
      phonetic.SetStringPath("text", phonetic_str);
      phonetics.Append(std::move(phonetic));
      entry.SetPath("phonetics", std::move(phonetics));
    }

    // Build definition.
    if (!definition.empty()) {
      Value sense_families(Value::Type::LIST);
      Value sense_family(Value::Type::DICTIONARY);
      Value senses(Value::Type::LIST);
      Value sense(Value::Type::DICTIONARY);
      sense.SetStringPath("definition.text", definition);
      senses.Append(std::move(sense));
      sense_family.SetPath("senses", std::move(senses));
      sense_families.Append(std::move(sense_family));
      entry.SetPath("senseFamilies", std::move(sense_families));
    }

    entries.Append(std::move(entry));

    result.SetPath("dictionaryResult.entries", std::move(entries));

    return result;
  }

  std::unique_ptr<DefinitionResultParser> parser_;
};

TEST_F(DefinitionResultParserTest, Success) {
  Value result =
      BuildDictionaryResult("unfathomable", "ˌənˈfaT͟Həməb(ə)",
                            "incapable of being fully explored or understood.");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));

  const auto& expected_title = "unfathomable · /ˌənˈfaT͟Həməb(ə)/";
  const auto& expected_answer =
      "incapable of being fully explored or understood.";
  EXPECT_EQ(ResultType::kDefinitionResult, quick_answer.result_type);
  EXPECT_EQ(expected_answer, quick_answer.primary_answer);
  EXPECT_EQ(expected_title, quick_answer.secondary_answer);

  EXPECT_EQ(1u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(base::UTF8ToUTF16(expected_answer), answer->text);
  EXPECT_EQ(gfx::kGoogleGrey700, answer->color);
  auto* title = static_cast<QuickAnswerText*>(quick_answer.title[0].get());
  EXPECT_EQ(base::UTF8ToUTF16(expected_title), title->text);
  EXPECT_EQ(gfx::kGoogleGrey900, title->color);
}

TEST_F(DefinitionResultParserTest, EmptyValue) {
  Value result(Value::Type::DICTIONARY);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoQueryTerm) {
  Value result =
      BuildDictionaryResult("", "ˌənˈfaT͟Həməb(ə)l",
                            "incapable of being fully explored or understood.");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(DefinitionResultParserTest, NoPhonetic) {
  Value result = BuildDictionaryResult(
      "unfathomable", "", "incapable of being fully explored or understood.");
  QuickAnswer quick_answer;
  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));

  const auto& expected_title = "unfathomable";
  const auto& expected_answer =
      "incapable of being fully explored or understood.";
  EXPECT_EQ(ResultType::kDefinitionResult, quick_answer.result_type);
  EXPECT_EQ(expected_answer, quick_answer.primary_answer);
  EXPECT_EQ(expected_title, quick_answer.secondary_answer);
}

TEST_F(DefinitionResultParserTest, NoDefinition) {
  Value result = BuildDictionaryResult("unfathomable", "ˌənˈfaT͟Həməb(ə)l", "");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
