// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_model.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_controller.h"
#include "components/omnibox/browser/test_omnibox_view.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

class TestOmniboxPopupView : public OmniboxPopupView {
 public:
  ~TestOmniboxPopupView() override {}
  bool IsOpen() const override { return false; }
  void InvalidateLine(size_t line) override {}
  void UpdatePopupAppearance() override {}
  void ProvideButtonFocusHint(size_t line) override {}
  void OnMatchIconUpdated(size_t match_index) override {}
  void OnDragCanceled() override {}
};

class TestOmniboxEditModel : public OmniboxEditModel {
 public:
  TestOmniboxEditModel(OmniboxView* view,
                       OmniboxEditController* controller,
                       std::unique_ptr<OmniboxClient> client)
      : OmniboxEditModel(view, controller, std::move(client)) {}
  bool PopupIsOpen() const override { return true; }
};

}  // namespace

using Selection = OmniboxPopupModel::Selection;

class OmniboxPopupModelTest : public ::testing::Test {
 public:
  OmniboxPopupModelTest()
      : view_(&controller_),
        model_(&view_, &controller_, std::make_unique<TestOmniboxClient>()),
        popup_model_(&popup_view_, &model_, &pref_service_) {
    omnibox::RegisterProfilePrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  OmniboxEditModel* model() { return &model_; }
  OmniboxPopupModel* popup_model() { return &popup_model_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestOmniboxEditController controller_;
  TestingPrefServiceSimple pref_service_;

  TestOmniboxView view_;
  TestOmniboxEditModel model_;
  TestOmniboxPopupView popup_view_;
  OmniboxPopupModel popup_model_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupModelTest);
};

// This verifies that the new treatment of the user's selected match in
// |SetSelectedLine()| with removed |AutocompleteResult::Selection::empty()|
// is correct in the face of various replacement versions of |empty()|.
TEST_F(OmniboxPopupModelTest, SetSelectedLine) {
  ACMatches matches;
  for (size_t i = 0; i < 2; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }
  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_FALSE(popup_model()->has_selected_match());
  popup_model()->SetSelectedLine(0, true, false);
  EXPECT_FALSE(popup_model()->has_selected_match());
  popup_model()->SetSelectedLine(0, false, false);
  EXPECT_TRUE(popup_model()->has_selected_match());
}

TEST_F(OmniboxPopupModelTest, SetSelectedLineWithNoDefaultMatches) {
  // Creates a set of matches with NO matches allowed to be default.
  ACMatches matches;
  for (size_t i = 0; i < 2; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    matches.push_back(match);
  }
  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);

  popup_model()->OnResultChanged();
  EXPECT_EQ(OmniboxPopupModel::kNoMatch, popup_model()->selected_line());
  EXPECT_FALSE(popup_model()->has_selected_match());

  popup_model()->SetSelectedLine(0, false, false);
  EXPECT_EQ(0U, popup_model()->selected_line());
  EXPECT_TRUE(popup_model()->has_selected_match());

  popup_model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(1U, popup_model()->selected_line());
  EXPECT_TRUE(popup_model()->has_selected_match());

  popup_model()->ResetToInitialState();
  EXPECT_EQ(OmniboxPopupModel::kNoMatch, popup_model()->selected_line());
  EXPECT_FALSE(popup_model()->has_selected_match());
}

TEST_F(OmniboxPopupModelTest, PopupPositionChanging) {
  ACMatches matches;
  for (size_t i = 0; i < 3; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }
  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0u, model()->popup_model()->selected_line());
  // Test moving and wrapping down.
  for (size_t n : {1, 2, 0}) {
    model()->OnUpOrDownKeyPressed(1);
    EXPECT_EQ(n, model()->popup_model()->selected_line());
  }
  // And down.
  for (size_t n : {2, 1, 0}) {
    model()->OnUpOrDownKeyPressed(-1);
    EXPECT_EQ(n, model()->popup_model()->selected_line());
  }
}

TEST_F(OmniboxPopupModelTest, PopupStepSelection) {
  ACMatches matches;
  for (size_t i = 0; i < 4; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }
  // Make match index 1 deletable to verify we can step to that.
  matches[1].deletable = true;
  // Make match index 2 have an associated keyword for irregular state stepping.
  // Make it deleteable also to verify that we correctly handle matches with
  // keywords that are ALSO deleteable (this is an edge case that was broken).
  matches[2].associated_keyword =
      std::make_unique<AutocompleteMatch>(matches.back());
  matches[2].deletable = true;
  // Make match index 3 have a suggestion_group_id to test header behavior.
  matches[3].suggestion_group_id = 7;

  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0u, model()->popup_model()->selected_line());

  // Step by lines forward.
  for (size_t n : {1, 2, 3, 0}) {
    popup_model()->StepSelection(OmniboxPopupModel::kForward,
                                 OmniboxPopupModel::kWholeLine);
    EXPECT_EQ(n, model()->popup_model()->selected_line());
  }
  // Step by lines backward.
  for (size_t n : {3, 2, 1, 0}) {
    popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                                 OmniboxPopupModel::kWholeLine);
    EXPECT_EQ(n, model()->popup_model()->selected_line());
  }
  // Step by states forward.
  for (auto selection : {
           Selection(1, OmniboxPopupModel::NORMAL),
           // Focused button is the Suggestion Removal button.
           Selection(1, OmniboxPopupModel::BUTTON_FOCUSED),
           Selection(2, OmniboxPopupModel::NORMAL),
           Selection(2, OmniboxPopupModel::KEYWORD),
           Selection(3, OmniboxPopupModel::HEADER_BUTTON_FOCUSED),
           Selection(3, OmniboxPopupModel::NORMAL),
           Selection(0, OmniboxPopupModel::NORMAL),
       }) {
    popup_model()->StepSelection(OmniboxPopupModel::kForward,
                                 OmniboxPopupModel::kStateOrLine);
    EXPECT_EQ(selection, model()->popup_model()->selection());
  }
  // Step by states backward.
  // Note the lack of KEYWORD. This is by design. Stepping forward
  // should land on KEYWORD, but stepping backward should not.
  for (auto selection : {
           Selection(3, OmniboxPopupModel::NORMAL),
           Selection(3, OmniboxPopupModel::HEADER_BUTTON_FOCUSED),
           Selection(2, OmniboxPopupModel::NORMAL),
           // Focused button is the Suggestion Removal button.
           Selection(1, OmniboxPopupModel::BUTTON_FOCUSED),
           Selection(1, OmniboxPopupModel::NORMAL),
           Selection(0, OmniboxPopupModel::NORMAL),
           Selection(3, OmniboxPopupModel::NORMAL),
           Selection(3, OmniboxPopupModel::HEADER_BUTTON_FOCUSED),
           Selection(2, OmniboxPopupModel::NORMAL),
       }) {
    popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                                 OmniboxPopupModel::kStateOrLine);
    EXPECT_EQ(selection, model()->popup_model()->selection());
  }

  // Try some kStateOrNothing steps on the keyword line.
  // Note that keyword mode is specially excepted with this
  // step behavior.
  popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                               OmniboxPopupModel::kStateOrNothing);
  EXPECT_EQ(Selection(2, OmniboxPopupModel::NORMAL),
            model()->popup_model()->selection());
  popup_model()->StepSelection(OmniboxPopupModel::kForward,
                               OmniboxPopupModel::kStateOrNothing);
  EXPECT_EQ(Selection(2, OmniboxPopupModel::NORMAL),
            model()->popup_model()->selection());

  // Try kStateOrNothing on the removable line, specifically, verify that
  // kStateOrNothing doesn't wrap between button and non-button focus.
  popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                               OmniboxPopupModel::kWholeLine);
  EXPECT_EQ(Selection(1, OmniboxPopupModel::NORMAL),
            model()->popup_model()->selection());
  popup_model()->StepSelection(OmniboxPopupModel::kForward,
                               OmniboxPopupModel::kStateOrNothing);
  EXPECT_EQ(Selection(1, OmniboxPopupModel::BUTTON_FOCUSED),
            model()->popup_model()->selection());
  // Verify that another step forward doesn't wrap back to the NORMAL state.
  popup_model()->StepSelection(OmniboxPopupModel::kForward,
                               OmniboxPopupModel::kStateOrNothing);
  EXPECT_EQ(Selection(1, OmniboxPopupModel::BUTTON_FOCUSED),
            model()->popup_model()->selection());

  // Try the kAllLines step behavior.
  popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                               OmniboxPopupModel::kAllLines);
  EXPECT_EQ(Selection(0, OmniboxPopupModel::NORMAL),
            model()->popup_model()->selection());
  popup_model()->StepSelection(OmniboxPopupModel::kForward,
                               OmniboxPopupModel::kAllLines);
  EXPECT_EQ(Selection(3, OmniboxPopupModel::NORMAL),
            model()->popup_model()->selection());
}

TEST_F(OmniboxPopupModelTest, PopupStepSelectionWithHiddenGroupIds) {
  ACMatches matches;
  for (size_t i = 0; i < 4; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = base::ASCIIToUTF16("match");
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }

  // Hide the second two matches.
  matches[2].suggestion_group_id = 7;
  matches[3].suggestion_group_id = 7;
  omnibox::ToggleSuggestionGroupIdVisibility(pref_service(), 7);

  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0u, model()->popup_model()->selected_line());

  // Test the simple kAllLines case.
  popup_model()->StepSelection(OmniboxPopupModel::kForward,
                               OmniboxPopupModel::kAllLines);
  EXPECT_EQ(1u, model()->popup_model()->selected_line());
  popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                               OmniboxPopupModel::kAllLines);
  EXPECT_EQ(0u, model()->popup_model()->selected_line());

  // Test the kStateOrLine case, forwards and backwards.
  for (auto selection : {
           Selection(1, OmniboxPopupModel::NORMAL),
           Selection(2, OmniboxPopupModel::HEADER_BUTTON_FOCUSED),
           Selection(0, OmniboxPopupModel::NORMAL),
       }) {
    popup_model()->StepSelection(OmniboxPopupModel::kForward,
                                 OmniboxPopupModel::kStateOrLine);
    EXPECT_EQ(selection, model()->popup_model()->selection());
  }
  for (auto selection : {
           Selection(2, OmniboxPopupModel::HEADER_BUTTON_FOCUSED),
           Selection(1, OmniboxPopupModel::NORMAL),
       }) {
    popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                                 OmniboxPopupModel::kStateOrLine);
    EXPECT_EQ(selection, model()->popup_model()->selection());
  }

  // Test the kStateOrNothing case (that it can't go to the header).
  popup_model()->StepSelection(OmniboxPopupModel::kForward,
                               OmniboxPopupModel::kStateOrNothing);
  EXPECT_EQ(Selection(1, OmniboxPopupModel::NORMAL),
            model()->popup_model()->selection());

  // Test the kWholeLine case, forwards and backwards.
  for (auto selection : {
           Selection(0, OmniboxPopupModel::NORMAL),
           Selection(1, OmniboxPopupModel::NORMAL),
       }) {
    popup_model()->StepSelection(OmniboxPopupModel::kForward,
                                 OmniboxPopupModel::kWholeLine);
    EXPECT_EQ(selection, model()->popup_model()->selection());
  }
  for (auto selection : {
           Selection(0, OmniboxPopupModel::NORMAL),
           Selection(1, OmniboxPopupModel::NORMAL),
       }) {
    popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                                 OmniboxPopupModel::kWholeLine);
    EXPECT_EQ(selection, model()->popup_model()->selection());
  }
}

TEST_F(OmniboxPopupModelTest, ComputeMatchMaxWidths) {
  int contents_max_width, description_max_width;
  const int separator_width = 10;
  const int kMinimumContentsWidth = 300;
  int contents_width, description_width, available_width;

  // Both contents and description fit fine.
  contents_width = 100;
  description_width = 50;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Contents should be given as much space as it wants up to 300 pixels.
  contents_width = 100;
  description_width = 50;
  available_width = 100;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // Description should be hidden if it's at least 75 pixels wide but doesn't
  // get 75 pixels of space.
  contents_width = 300;
  description_width = 100;
  available_width = 384;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // If contents and description are on separate lines, each can take the full
  // available width.
  contents_width = 300;
  description_width = 100;
  available_width = 384;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Both contents and description will be limited.
  contents_width = 310;
  description_width = 150;
  available_width = 400;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(kMinimumContentsWidth, contents_max_width);
  EXPECT_EQ(available_width - kMinimumContentsWidth - separator_width,
            description_max_width);

  // Contents takes all available space.
  contents_width = 400;
  description_width = 0;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(available_width, contents_max_width);
  EXPECT_EQ(0, description_max_width);

  // Large contents will be truncated but small description won't if two line
  // suggestion.
  contents_width = 400;
  description_width = 100;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(available_width, contents_max_width);
  EXPECT_EQ(description_width, description_max_width);

  // Large description will be truncated but small contents won't if two line
  // suggestion.
  contents_width = 100;
  description_width = 400;
  available_width = 200;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width, true,
      true, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ(available_width, description_max_width);

  // Half and half.
  contents_width = 395;
  description_width = 395;
  available_width = 700;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(345, contents_max_width);
  EXPECT_EQ(345, description_max_width);

  // When we disallow shrinking the contents, it should get as much space as
  // it wants.
  contents_width = 395;
  description_width = 395;
  available_width = 700;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, false, &contents_max_width, &description_max_width);
  EXPECT_EQ(contents_width, contents_max_width);
  EXPECT_EQ((available_width - contents_width - separator_width),
            description_max_width);

  // (available_width - separator_width) is odd, so contents gets the extra
  // pixel.
  contents_width = 395;
  description_width = 395;
  available_width = 699;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(345, contents_max_width);
  EXPECT_EQ(344, description_max_width);

  // Not enough space to draw anything.
  contents_width = 1;
  description_width = 1;
  available_width = 0;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      contents_width, separator_width, description_width, available_width,
      false, true, &contents_max_width, &description_max_width);
  EXPECT_EQ(0, contents_max_width);
  EXPECT_EQ(0, description_max_width);
}

// Makes sure focus remains on the tab switch button when nothing changes,
// and leaves when it does. Exercises the ratcheting logic in
// OmniboxPopupModel::OnResultChanged().
TEST_F(OmniboxPopupModelTest, TestFocusFixing) {
  ACMatches matches;
  AutocompleteMatch match(nullptr, 1000, false,
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  match.contents = base::ASCIIToUTF16("match1.com");
  match.destination_url = GURL("http://match1.com");
  match.allowed_to_be_default_match = true;
  match.has_tab_match = true;
  matches.push_back(match);

  auto* result = &model()->autocomplete_controller()->result_;
  AutocompleteInput input(base::UTF8ToUTF16("match"),
                          metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  popup_model()->SetSelectedLine(0, true, false);
  // The default state should be unfocused.
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Focus the selection.
  popup_model()->SetSelectedLine(0, false, false);
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  EXPECT_EQ(OmniboxPopupModel::BUTTON_FOCUSED,
            popup_model()->selected_line_state());

  // Adding a match at end won't change that we selected first suggestion, so
  // shouldn't change focused state.
  matches[0].relevance = 999;
  // Give it a different name so not deduped.
  matches[0].contents = base::ASCIIToUTF16("match2.com");
  matches[0].destination_url = GURL("http://match2.com");
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(OmniboxPopupModel::BUTTON_FOCUSED,
            popup_model()->selected_line_state());

  // Changing selection should change focused state.
  popup_model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Changing selection to same selection might change state.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  // Letting routine filter selecting same line should not change it.
  popup_model()->SetSelectedLine(1, false, false);
  EXPECT_EQ(OmniboxPopupModel::BUTTON_FOCUSED,
            popup_model()->selected_line_state());
  // Forcing routine to handle selecting same line should change it.
  popup_model()->SetSelectedLine(1, false, true);
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Adding a match at end will reset selection to first, so should change
  // selected line, and thus focus.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  matches[0].relevance = 999;
  matches[0].contents = base::ASCIIToUTF16("match3.com");
  matches[0].destination_url = GURL("http://match3.com");
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0U, popup_model()->selected_line());
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Prepending a match won't change selection, but since URL is different,
  // should clear the focus state.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  matches[0].relevance = 1100;
  matches[0].contents = base::ASCIIToUTF16("match4.com");
  matches[0].destination_url = GURL("http://match4.com");
  result->AppendMatches(input, matches);
  result->SortAndCull(input, nullptr);
  popup_model()->OnResultChanged();
  EXPECT_EQ(0U, popup_model()->selected_line());
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());

  // Selecting |kNoMatch| should clear focus.
  popup_model()->SetSelectedLineState(OmniboxPopupModel::BUTTON_FOCUSED);
  popup_model()->SetSelectedLine(OmniboxPopupModel::kNoMatch, false, false);
  popup_model()->OnResultChanged();
  EXPECT_EQ(OmniboxPopupModel::NORMAL, popup_model()->selected_line_state());
}
