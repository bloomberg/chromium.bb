// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/multi_word_suggester.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/services/ime/public/cpp/suggestions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

constexpr int kFocusedContextId = 5;

void SendKeyEvent(MultiWordSuggester* suggester, const ui::DomCode& code) {
  suggester->HandleKeyEvent(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                         code, ui::EF_NONE, ui::DomKey::NONE,
                                         ui::EventTimeForNow()));
}

void SetFirstAcceptTimeTo(Profile* profile, int days_ago) {
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  base::TimeDelta since_epoch = base::Time::Now() - base::Time::UnixEpoch();
  update->SetIntKey("multi_word_first_accept",
                    since_epoch.InDaysFloored() - days_ago);
}

absl::optional<int> GetFirstAcceptTime(Profile* profile) {
  DictionaryPrefUpdate update(profile->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  auto value = update->FindIntKey("multi_word_first_accept");
  if (value.has_value())
    return value.value();
  return absl::nullopt;
}

}  // namespace

class MultiWordSuggesterTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    suggester_ = std::make_unique<MultiWordSuggester>(&suggestion_handler_,
                                                      profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  FakeSuggestionHandler suggestion_handler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MultiWordSuggester> suggester_;
};

TEST_F(MultiWordSuggesterTest, IgnoresIrrelevantExternalSuggestions) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kAssistivePersonalInfo,
                     .text = "my name is John Wayne"}};

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler_.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, IgnoresEmpyExternalSuggestions) {
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated({});

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_NE(suggestion_handler_.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, DisplaysRelevantExternalSuggestions) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there!"}};

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetContextId(), kFocusedContextId);
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hello there!");
}

TEST_F(MultiWordSuggesterTest, AcceptsSuggestionOnTabPress) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetDismissedSuggestion());
  EXPECT_TRUE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnNonTabKeypress) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hi there!");
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnArrowDownKeypress) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hi there!");
}

TEST_F(MultiWordSuggesterTest, DoesNotAcceptSuggestionOnEnterKeypress) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ENTER);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hi there!");
}

TEST_F(MultiWordSuggesterTest, AcceptsSuggestionOnDownPlusEnterPress) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ENTER);

  EXPECT_FALSE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_FALSE(suggestion_handler_.GetDismissedSuggestion());
  EXPECT_TRUE(suggestion_handler_.GetAcceptedSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"");
}

TEST_F(MultiWordSuggesterTest, HighlightsSuggestionOnDownArrow) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, MaintainsHighlightOnMultipleDownArrow) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);

  EXPECT_TRUE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, RemovesHighlightOnDownThenUpArrow) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_DOWN);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, HighlightIsNotShownWithUpArrow) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, HighlightIsNotShownWithMultipleUpArrow) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);
  SendKeyEvent(suggester_.get(), ui::DomCode::ARROW_UP);

  EXPECT_FALSE(suggestion_handler_.GetHighlightedSuggestion());
}

TEST_F(MultiWordSuggesterTest, DisplaysTabGuideline) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  auto suggestion_details = suggestion_handler_.GetLastSuggestionDetails();
  EXPECT_TRUE(suggestion_details.show_quick_accept_annotation);
}

TEST_F(MultiWordSuggesterTest,
       DisplaysTabGuidelineWithinSevenDaysOfFirstAccept) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  SetFirstAcceptTimeTo(profile_.get(), /*days_ago=*/6);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  auto suggestion_details = suggestion_handler_.GetLastSuggestionDetails();
  EXPECT_TRUE(suggestion_details.show_quick_accept_annotation);
}

TEST_F(MultiWordSuggesterTest,
       DoesNotDisplayTabGuidelineSevenDaysAfterFirstAccept) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  SetFirstAcceptTimeTo(profile_.get(), /*days_ago=*/7);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  auto suggestion_details = suggestion_handler_.GetLastSuggestionDetails();
  EXPECT_FALSE(suggestion_details.show_quick_accept_annotation);
}

TEST_F(MultiWordSuggesterTest, SetsAcceptTimeOnFirstSuggestionAcceptedOnly) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hi there!"},
  };

  auto pref_before_accept = GetFirstAcceptTime(profile_.get());
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);
  auto pref_after_first_accept = GetFirstAcceptTime(profile_.get());

  suggester_->OnSurroundingTextChanged(u"", 0, 0);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);
  auto pref_after_second_accept = GetFirstAcceptTime(profile_.get());

  EXPECT_EQ(pref_before_accept, absl::nullopt);
  ASSERT_TRUE(pref_after_first_accept.has_value());
  ASSERT_TRUE(pref_after_second_accept.has_value());
  EXPECT_EQ(*pref_after_first_accept, *pref_after_second_accept);
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthForOneWord) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you going"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"ho", /*cursor_pos=*/2,
                                       /*anchor_pos=*/2);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you going");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 2);  // ho
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthForManyWords) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "where are you going"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hey there sam whe",
                                       /*cursor_pos=*/17, /*anchor_pos=*/17);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"where are you going");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 3);  // whe
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthGreedily) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hohohohoho"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"merry christmas hohoho",
                                       /*cursor_pos=*/22,
                                       /*anchor_pos=*/22);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"hohohohoho");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 6);  // hohoho
}

TEST_F(MultiWordSuggesterTest, CalculatesConfirmedLengthForPredictions) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "is the next task"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this ",
                                       /*cursor_pos=*/5, /*anchor_pos=*/5);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"is the next task");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 0);
}

TEST_F(MultiWordSuggesterTest, HandlesNewlinesWhenCalculatingConfirmedLength) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"\nh",
                                       /*cursor_pos=*/2, /*anchor_pos=*/2);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 1);  // h
}

TEST_F(MultiWordSuggesterTest, HandlesMultipleRepeatingCharsWhenTracking) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", /*cursor_pos=*/1,
                                       /*anchor_pos=*/1);
  suggester_->Suggest(u"h", /*cursor_pos=*/1, /*anchor_pos=*/1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"hh", /*cursor_pos=*/2,
                                       /*anchor_pos=*/2);

  EXPECT_FALSE(suggester_->Suggest(u"hh", /*cursor_pos=*/2, /*anchor_pos=*/2));
}

TEST_F(MultiWordSuggesterTest, DoesNotDismissOnMultipleCursorMoveToEndOfText) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hello h", /*cursor_pos=*/7,
                                       /*anchor_pos=*/7);
  suggester_->Suggest(u"hello h", /*cursor_pos=*/7, /*anchor_pos=*/7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"hello h", /*cursor_pos=*/7,
                                       /*anchor_pos=*/7);
  suggester_->Suggest(u"hello h", /*cursor_pos=*/7, /*anchor_pos=*/7);
  suggester_->OnSurroundingTextChanged(u"hello h", /*cursor_pos=*/7,
                                       /*anchor_pos=*/7);

  EXPECT_TRUE(suggester_->Suggest(u"hello h", /*cursor_pos=*/7,
                                  /*anchor_pos=*/7));
}

TEST_F(MultiWordSuggesterTest, TracksLastSuggestionOnSurroundingTextChange) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "where are you going"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hey there sam whe", 17, 17);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"hey there sam wher", 18, 18);
  suggester_->Suggest(u"hey there sam wher", 18, 18);
  suggester_->OnSurroundingTextChanged(u"hey there sam where", 19, 19);
  suggester_->Suggest(u"hey there sam where", 19, 19);
  suggester_->OnSurroundingTextChanged(u"hey there sam where ", 20, 20);
  suggester_->Suggest(u"hey there sam where ", 20, 20);
  suggester_->OnSurroundingTextChanged(u"hey there sam where a", 21, 21);
  suggester_->Suggest(u"hey there sam where a", 21, 21);
  suggester_->OnSurroundingTextChanged(u"hey there sam where ar", 22, 22);
  suggester_->Suggest(u"hey there sam where ar", 22, 22);
  suggester_->OnSurroundingTextChanged(u"hey there sam where are", 23, 23);
  suggester_->Suggest(u"hey there sam where are", 23, 23);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"where are you going");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 9);  // where are
}

TEST_F(MultiWordSuggesterTest,
       TracksLastSuggestionOnSurroundingTextChangeAtBeginningText) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"ho", 2, 2);
  suggester_->Suggest(u"ho", 2, 2);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->Suggest(u"how", 3, 3);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 3);  // how
}

TEST_F(MultiWordSuggesterTest,
       TracksLastSuggestionOnLargeSurroundingTextChange) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->Suggest(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are yo", 10, 10);
  suggester_->Suggest(u"how are yo", 10, 10);

  EXPECT_TRUE(suggestion_handler_.GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_.GetSuggestionText(), u"how are you");
  EXPECT_EQ(suggestion_handler_.GetConfirmedLength(), 10);  // how are yo
}

TEST_F(MultiWordSuggesterTest,
       DoesNotTrackLastSuggestionIfSurroundingTextChange) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->Suggest(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how yo", 6, 6);

  // The consumer will handle dismissing the suggestion
  EXPECT_FALSE(suggester_->Suggest(u"how yo", 6, 6));
}

TEST_F(MultiWordSuggesterTest,
       DoesNotTrackLastSuggestionIfCursorBeforeSuggestionStartPos) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = " for the example"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this is some text", 17, 17);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"this is some text ", 18, 18);
  suggester_->Suggest(u"this is some text ", 18, 18);
  suggester_->OnSurroundingTextChanged(u"this is some text f", 19, 19);
  suggester_->Suggest(u"this is some text f", 19, 19);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester_->Suggest(u"this is some text fo", 20, 20);
  suggester_->OnSurroundingTextChanged(u"this is some text f", 19, 19);
  suggester_->Suggest(u"this is some text f", 19, 19);
  suggester_->OnSurroundingTextChanged(u"this is some text ", 18, 18);
  suggester_->Suggest(u"this is some text ", 18, 18);
  suggester_->OnSurroundingTextChanged(u"this is some text", 17, 17);

  EXPECT_FALSE(suggester_->Suggest(u"this is some text", 17, 17));
}

TEST_F(MultiWordSuggesterTest, DoesNotTrackSuggestionPastSuggestionPoint) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = " for the example"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"this is some text for", 21, 21);
  suggester_->Suggest(u"this is some text for", 21, 21);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  bool at_suggestion_point =
      suggester_->Suggest(u"this is some text fo", 20, 20);
  suggester_->OnSurroundingTextChanged(u"this is some text f", 19, 19);
  bool before_suggestion_point =
      suggester_->Suggest(u"this is some text f", 19, 19);

  EXPECT_TRUE(at_suggestion_point);
  EXPECT_FALSE(before_suggestion_point);
}

TEST_F(MultiWordSuggesterTest,
       DismissesSuggestionAfterCursorMoveFromEndOfText) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = " for the example"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"this is some text fo", 20, 20);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"this is some text for", 21, 21);
  suggester_->Suggest(u"this is some text for", 21, 21);
  suggester_->OnSurroundingTextChanged(u"this is some text for", 15, 15);

  EXPECT_FALSE(suggester_->Suggest(u"this is some text for", 15, 15));
}

TEST_F(MultiWordSuggesterTest, DismissesSuggestionOnUserTypingFullSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = " are"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"how ", 4, 4);
  suggester_->Suggest(u"how ", 4, 4);
  suggester_->OnSurroundingTextChanged(u"how a", 5, 5);
  suggester_->Suggest(u"how a", 5, 5);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->Suggest(u"how ar", 6, 6);
  suggester_->OnSurroundingTextChanged(u"how are", 7, 7);

  EXPECT_FALSE(suggester_->Suggest(u"how are", 7, 7));
}

TEST_F(MultiWordSuggesterTest, ReturnsGenericActionIfNoSuggestionHasBeenShown) {
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"hey there sam whe", 17, 17);

  EXPECT_EQ(suggester_->GetProposeActionType(), AssistiveType::kGenericAction);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsCompletionActionIfCompletionSuggestionShown) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how ar", 6, 6);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordCompletion);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsPredictionActionIfPredictionSuggestionShown) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordPrediction);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsCompletionActionAfterAcceptingCompletionSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why ar", 6, 6);
  suggester_->Suggest(u"why", 6, 6);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  ASSERT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordCompletion);
}

TEST_F(MultiWordSuggesterTest,
       ReturnsPredictionActionAfterAcceptingPredictionSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why", 3, 3);
  suggester_->Suggest(u"why", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  ASSERT_EQ(suggester_->GetProposeActionType(),
            AssistiveType::kMultiWordPrediction);
}

TEST_F(MultiWordSuggesterTest, RecordsTimeToAcceptMetric) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.MultiWord", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  EXPECT_TRUE(suggestion_handler_.GetAcceptedSuggestion());
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.MultiWord", 1);
}

TEST_F(MultiWordSuggesterTest, RecordsTimeToDismissMetric) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "how are you"},
  };

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.MultiWord", 0);

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"how", 3, 3);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->DismissSuggestion();

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.MultiWord", 1);
}

TEST_F(MultiWordSuggesterTest,
       SurroundingTextChangesDoNotTriggerAnnouncements) {
  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->Suggest(u"why aren", 8, 8);
  suggester_->OnSurroundingTextChanged(u"why aren'", 9, 9);
  suggester_->Suggest(u"why aren'", 9, 9);
  suggester_->OnSurroundingTextChanged(u"why aren't", 10, 10);
  suggester_->Suggest(u"why aren't", 10, 10);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 0);
}

TEST_F(MultiWordSuggesterTest, ShowingSuggestionsTriggersAnnouncement) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 1);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate shown, press tab to accept");
}

TEST_F(MultiWordSuggesterTest,
       TrackingSuggestionsTriggersAnnouncementOnlyOnce) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->Suggest(u"why aren", 8, 8);
  suggester_->OnSurroundingTextChanged(u"why aren'", 9, 9);
  suggester_->Suggest(u"why aren'", 9, 9);
  suggester_->OnSurroundingTextChanged(u"why aren't", 10, 10);
  suggester_->Suggest(u"why aren't", 10, 10);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 1);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate shown, press tab to accept");
}

TEST_F(MultiWordSuggesterTest, AcceptingSuggestionTriggersAnnouncement) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate inserted");
}

TEST_F(MultiWordSuggesterTest,
       TransitionsFromAcceptSuggestionToNoSuggestionDoesNotTriggerAnnounce) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  SendKeyEvent(suggester_.get(), ui::DomCode::TAB);
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->Suggest(u"why aren", 8, 8);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2);
}

TEST_F(MultiWordSuggesterTest, DismissingSuggestionTriggersAnnouncement) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->DismissSuggestion();

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2);
  EXPECT_EQ(suggestion_handler_.GetAnnouncements().back(),
            u"predictive writing candidate dismissed");
}

TEST_F(MultiWordSuggesterTest,
       TransitionsFromDismissSuggestionToNoSuggestionDoesNotTriggerAnnounce) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"},
  };

  suggester_->OnFocus(kFocusedContextId);
  suggester_->OnSurroundingTextChanged(u"why are", 7, 7);
  suggester_->Suggest(u"why are", 7, 7);
  suggester_->OnExternalSuggestionsUpdated(suggestions);
  suggester_->DismissSuggestion();
  suggester_->OnSurroundingTextChanged(u"why aren", 8, 8);
  suggester_->Suggest(u"why aren", 8, 8);

  ASSERT_EQ(suggestion_handler_.GetAnnouncements().size(), 2);
}

}  // namespace input_method
}  // namespace ash
