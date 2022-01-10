// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/suggestions_collector.h"

#include <vector>

#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::ime::TextCompletionCandidate;
using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

class FakeAssistiveSuggester : public SuggestionsSource {
 public:
  std::vector<TextSuggestion> GetSuggestions() override { return suggestions_; }

  void SetSuggestions(const std::vector<TextSuggestion> suggestions) {
    suggestions_ = suggestions;
  }

 private:
  std::vector<TextSuggestion> suggestions_;
};

class FakeSuggestionsService : public AsyncSuggestionsSource {
 public:
  void RequestSuggestions(
      const std::string& preceding_text,
      const ime::TextSuggestionMode& suggestion_mode,
      const std::vector<TextCompletionCandidate>& completion_candidates,
      RequestSuggestionsCallback callback) override {
    std::move(callback).Run(suggestions_);
  }

  bool IsAvailable() override { return is_available_; }

  void SetSuggestions(const std::vector<TextSuggestion> suggestions) {
    suggestions_ = suggestions;
  }

  void SetIsAvailable(bool is_available) { is_available_ = is_available; }

 private:
  std::vector<TextSuggestion> suggestions_;
  bool is_available_ = true;
};

class SuggestionsCollectorTest : public ::testing::Test {
 public:
  void SetUp() override {
    multi_word_result_ = TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                                        .type = TextSuggestionType::kMultiWord,
                                        .text = "hello there"};

    personal_info_name_result_ =
        TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                       .type = TextSuggestionType::kAssistivePersonalInfo,
                       .text = "my name is Mr Robot"};

    personal_info_address_result_ =
        TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                       .type = TextSuggestionType::kAssistivePersonalInfo,
                       .text = "my address is 123 Fake St"};
  }

  std::vector<TextSuggestion> suggestions_returned() {
    return suggestions_returned_;
  }

  TextSuggestion multi_word_result() { return multi_word_result_; }
  TextSuggestion personal_info_name_result() {
    return personal_info_name_result_;
  }
  TextSuggestion personal_info_address_result() {
    return personal_info_address_result_;
  }

  void OnSuggestionsReturned(
      chromeos::ime::mojom::SuggestionsResponsePtr response) {
    suggestions_returned_ = response->candidates;
  }

 private:
  std::vector<TextSuggestion> suggestions_returned_;
  TextSuggestion multi_word_result_;
  TextSuggestion personal_info_name_result_;
  TextSuggestion personal_info_address_result_;
};

TEST_F(SuggestionsCollectorTest, ReturnsResultsFromAssistiveSuggester) {
  FakeAssistiveSuggester suggester;
  auto requestor = std::make_unique<FakeSuggestionsService>();

  auto expected_results = std::vector<TextSuggestion>{
      personal_info_name_result(),
      personal_info_address_result(),
  };

  suggester.SetSuggestions(expected_results);
  SuggestionsCollector collector(&suggester, std::move(requestor));

  collector.GatherSuggestions(
      chromeos::ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

TEST_F(SuggestionsCollectorTest, ReturnsResultsFromSuggestionsRequestor) {
  FakeAssistiveSuggester suggester;
  auto requestor = std::make_unique<FakeSuggestionsService>();

  auto expected_results = std::vector<TextSuggestion>{multi_word_result()};

  requestor->SetSuggestions(expected_results);
  SuggestionsCollector collector(&suggester, std::move(requestor));

  collector.GatherSuggestions(
      chromeos::ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

TEST_F(SuggestionsCollectorTest, ReturnsCombinedResultsIfAvailable) {
  FakeAssistiveSuggester assistive_suggester;
  auto suggestions_requestor = std::make_unique<FakeSuggestionsService>();

  suggestions_requestor->SetSuggestions({multi_word_result()});
  assistive_suggester.SetSuggestions(
      {personal_info_name_result(), personal_info_address_result()});

  SuggestionsCollector collector(&assistive_suggester,
                                 std::move(suggestions_requestor));

  auto expected_results = std::vector<TextSuggestion>{
      personal_info_name_result(),
      personal_info_address_result(),
      multi_word_result(),
  };

  collector.GatherSuggestions(
      chromeos::ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

TEST_F(SuggestionsCollectorTest,
       OnlyReturnsAssistiveResultsIfRequestorNotAvailable) {
  FakeAssistiveSuggester assistive_suggester;
  auto suggestions_requestor = std::make_unique<FakeSuggestionsService>();

  suggestions_requestor->SetSuggestions({multi_word_result()});
  suggestions_requestor->SetIsAvailable(false);
  assistive_suggester.SetSuggestions(
      {personal_info_name_result(), personal_info_address_result()});

  SuggestionsCollector collector(&assistive_suggester,
                                 std::move(suggestions_requestor));

  auto expected_results = std::vector<TextSuggestion>{
      personal_info_name_result(),
      personal_info_address_result(),
  };

  collector.GatherSuggestions(
      chromeos::ime::mojom::SuggestionsRequest::New(),
      base::BindOnce(&SuggestionsCollectorTest::OnSuggestionsReturned,
                     base::Unretained(this)));

  EXPECT_EQ(suggestions_returned(), expected_results);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
