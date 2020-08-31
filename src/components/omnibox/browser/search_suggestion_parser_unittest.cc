// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_suggestion_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

////////////////////////////////////////////////////////////////////////////////
// DeserializeJsonData:

TEST(SearchSuggestionParserTest, DeserializeNonListJsonIsInvalid) {
  std::string json_data = "{}";
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_FALSE(result);
}

TEST(SearchSuggestionParserTest, DeserializeMalformedJsonIsInvalid) {
  std::string json_data = "} malformed json {";
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_FALSE(result);
}

TEST(SearchSuggestionParserTest, DeserializeJsonData) {
  std::string json_data = R"([{"one": 1}])";
  base::Optional<base::Value> manifest_value =
      base::JSONReader::Read(json_data);
  ASSERT_TRUE(manifest_value);
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
  ASSERT_EQ(*manifest_value, *result);
}

TEST(SearchSuggestionParserTest, DeserializeWithXssiGuard) {
  // For XSSI protection, non-json may precede the actual data.
  // Parsing fails at:       v         v
  std::string json_data = R"([non-json [prefix [{"one": 1}])";
  // Parsing succeeds at:                      ^

  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);

  // Specifically, we precede JSON with )]}'\n.
  json_data = ")]}'\n[{\"one\": 1}]";
  result = SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
}

TEST(SearchSuggestionParserTest, DeserializeWithTrailingComma) {
  // The comma in this string makes this badly formed JSON, but we explicitly
  // allow for this error in the JSON data.
  std::string json_data = R"([{"one": 1},])";
  std::unique_ptr<base::Value> result =
      SearchSuggestionParser::DeserializeJsonData(json_data);
  ASSERT_TRUE(result);
}

////////////////////////////////////////////////////////////////////////////////
// ExtractJsonData:

// TODO(crbug.com/831283): Add some ExtractJsonData tests.

////////////////////////////////////////////////////////////////////////////////
// ParseSuggestResults:

TEST(SearchSuggestionParserTest, ParseEmptyValueIsInvalid) {
  base::Value root_val;
  AutocompleteInput input;
  TestSchemeClassifier scheme_classifier;
  int default_result_relevance = 0;
  bool is_keyword_result = false;
  SearchSuggestionParser::Results results;
  ASSERT_FALSE(SearchSuggestionParser::ParseSuggestResults(
      root_val, input, scheme_classifier, default_result_relevance,
      is_keyword_result, &results));
}

TEST(SearchSuggestionParserTest, ParseNonSuggestionValueIsInvalid) {
  std::string json_data = R"([{"one": 1}])";
  base::Optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  AutocompleteInput input;
  TestSchemeClassifier scheme_classifier;
  int default_result_relevance = 0;
  bool is_keyword_result = false;
  SearchSuggestionParser::Results results;
  ASSERT_FALSE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, default_result_relevance,
      is_keyword_result, &results));
}

TEST(SearchSuggestionParserTest, ParseSuggestResults) {
  std::string json_data = R"([
      "chris",
      ["christmas", "christopher doe"],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:fieldtrialtriggered": true,
        "google:suggestdetail": [{
          }, {
            "a": "American author",
            "dc": "#424242",
            "i": "http://example.com/a.png",
            "q": "gs_ssp=abc",
            "t": "Christopher Doe"
          }],
        "google:suggestrelevance": [607, 606],
        "google:suggesttype": ["QUERY", "ENTITY"],
        "google:verbatimrelevance": 851
      }])";
  base::Optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(base::ASCIIToUTF16("chris"),
                          metrics::OmniboxEventProto::NTP, scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));
  // We have "google:suggestrelevance".
  ASSERT_EQ(true, results.relevances_from_server);
  // We have "google:fieldtrialtriggered".
  ASSERT_EQ(true, results.field_trial_triggered);
  // The "google:verbatimrelevance".
  ASSERT_EQ(851, results.verbatim_relevance);
  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(base::ASCIIToUTF16("christmas"), suggestion_result.suggestion());
    ASSERT_EQ(base::ASCIIToUTF16(""), suggestion_result.annotation());
    // This entry has no image.
    ASSERT_EQ("", suggestion_result.image_dominant_color());
    ASSERT_EQ(GURL(), suggestion_result.image_url());
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(base::ASCIIToUTF16("christopher doe"),
              suggestion_result.suggestion());
    ASSERT_EQ(base::ASCIIToUTF16("American author"),
              suggestion_result.annotation());
    ASSERT_EQ("#424242", suggestion_result.image_dominant_color());
    ASSERT_EQ(GURL("http://example.com/a.png"), suggestion_result.image_url());
  }
}

TEST(SearchSuggestionParserTest, SuggestClassification) {
  SearchSuggestionParser::SuggestResult result(
      base::ASCIIToUTF16("foobar"), AutocompleteMatchType::SEARCH_SUGGEST, 0,
      false, 400, true, base::string16());
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());

  // Nothing should be bolded for ZeroSuggest classified input.
  result.ClassifyMatchContents(true, base::string16());
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kNone = {
      {0, AutocompleteMatch::ACMatchClassification::NONE}};
  EXPECT_EQ(kNone, result.match_contents_class());

  // Test a simple case of bolding half the text.
  result.ClassifyMatchContents(false, base::ASCIIToUTF16("foo"));
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kHalfBolded = {
      {0, AutocompleteMatch::ACMatchClassification::NONE},
      {3, AutocompleteMatch::ACMatchClassification::MATCH}};
  EXPECT_EQ(kHalfBolded, result.match_contents_class());

  // Test the edge case that if we forbid bolding all, and then reclassifying
  // would otherwise bold-all, we leave the existing classifications alone.
  // This is weird, but it's in the function contract, and is useful for
  // flicker-free search suggestions as the user types.
  result.ClassifyMatchContents(false, base::ASCIIToUTF16("apple"));
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  EXPECT_EQ(kHalfBolded, result.match_contents_class());

  // And finally, test the case where we do allow bolding-all.
  result.ClassifyMatchContents(true, base::ASCIIToUTF16("apple"));
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kBoldAll = {
      {0, AutocompleteMatch::ACMatchClassification::MATCH}};
  EXPECT_EQ(kBoldAll, result.match_contents_class());
}

TEST(SearchSuggestionParserTest, NavigationClassification) {
  TestSchemeClassifier scheme_classifier;
  SearchSuggestionParser::NavigationResult result(
      scheme_classifier, GURL("https://news.google.com/"),
      AutocompleteMatchType::Type::NAVSUGGEST, 0, base::string16(),
      std::string(), false, 400, true, base::ASCIIToUTF16("google"));
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kBoldMiddle = {
      {0, AutocompleteMatch::ACMatchClassification::URL},
      {5, AutocompleteMatch::ACMatchClassification::URL |
              AutocompleteMatch::ACMatchClassification::MATCH},
      {11, AutocompleteMatch::ACMatchClassification::URL}};
  EXPECT_EQ(kBoldMiddle, result.match_contents_class());

  // Reclassifying in a way that would cause bold-none if it's disallowed should
  // do nothing.
  result.CalculateAndClassifyMatchContents(
      false, base::ASCIIToUTF16("term not found"));
  EXPECT_EQ(kBoldMiddle, result.match_contents_class());

  // Test the allow bold-nothing case too.
  result.CalculateAndClassifyMatchContents(
      true, base::ASCIIToUTF16("term not found"));
  const ACMatchClassifications kAnnotateUrlOnly = {
      {0, AutocompleteMatch::ACMatchClassification::URL}};
  EXPECT_EQ(kAnnotateUrlOnly, result.match_contents_class());

  // Nothing should be bolded for ZeroSuggest classified input.
  result.CalculateAndClassifyMatchContents(true, base::string16());
  AutocompleteMatch::ValidateClassifications(result.match_contents(),
                                             result.match_contents_class());
  const ACMatchClassifications kNone = {
      {0, AutocompleteMatch::ACMatchClassification::NONE}};
  EXPECT_EQ(kNone, result.match_contents_class());
}

TEST(SearchSuggestionParserTest, ParseHeaderInfo) {
  std::string json_data = R"([
      "",
      ["los angeles", "san diego", "las vegas", "san francisco"],
      ["history", "", "", ""],
      [],
      {
        "google:clientdata": {
          "bpc": false,
          "tlw": false
        },
        "google:headertexts":{
          "a":{
            "40007":"Not recommended for you",
            "40008":"Recommended for you"
          }
        },
        "google:suggestdetail":[
          {
          },
          {
            "zl":40007
          },
          {
            "zl":40008
          },
          {
            "zl":40009
          }
        ],
        "google:suggestrelevance": [607, 606, 605, 604],
        "google:suggesttype": ["PERSONALIZED_QUERY", "QUERY", "QUERY", "QUERY"]
      }])";
  base::Optional<base::Value> root_val = base::JSONReader::Read(json_data);
  ASSERT_TRUE(root_val);
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(base::ASCIIToUTF16(""),
                          metrics::OmniboxEventProto::NTP_REALBOX,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(SearchSuggestionParser::ParseSuggestResults(
      *root_val, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, &results));

  {
    const auto& suggestion_result = results.suggest_results[0];
    ASSERT_EQ(base::ASCIIToUTF16("los angeles"),
              suggestion_result.suggestion());
    // This suggestion does not belong to a group.
    ASSERT_EQ(base::nullopt, suggestion_result.suggestion_group_id());
  }
  {
    const auto& suggestion_result = results.suggest_results[1];
    ASSERT_EQ(base::ASCIIToUTF16("san diego"), suggestion_result.suggestion());
    ASSERT_EQ(40007, *suggestion_result.suggestion_group_id());
  }
  {
    const auto& suggestion_result = results.suggest_results[2];
    ASSERT_EQ(base::ASCIIToUTF16("las vegas"), suggestion_result.suggestion());
    ASSERT_EQ(40008, *suggestion_result.suggestion_group_id());
  }
  {
    const auto& suggestion_result = results.suggest_results[3];
    ASSERT_EQ(base::ASCIIToUTF16("san francisco"),
              suggestion_result.suggestion());
    ASSERT_EQ(40009, *suggestion_result.suggestion_group_id());
  }
}
