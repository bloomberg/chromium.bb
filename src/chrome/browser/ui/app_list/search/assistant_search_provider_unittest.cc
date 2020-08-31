// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/test_support/mock_assistant_controller.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/assistant_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace test {

using chromeos::assistant::AssistantAllowedState;
using chromeos::assistant::mojom::AssistantEntryPoint;
using chromeos::assistant::mojom::AssistantQuerySource;
using chromeos::assistant::mojom::AssistantSuggestion;
using chromeos::assistant::mojom::AssistantSuggestionPtr;
using testing::DoAll;
using testing::NiceMock;
using testing::SaveArg;

// Expectations ----------------------------------------------------------------

class Expect {
 public:
  explicit Expect(const ChromeSearchResult& result) : r_(result) {
    EXPECT_EQ(r_.display_index(), ash::SearchResultDisplayIndex::kFirstIndex);
    EXPECT_EQ(r_.display_type(), ash::SearchResultDisplayType::kChip);
    EXPECT_EQ(r_.result_type(), ash::AppListSearchResultType::kAssistantChip);
    EXPECT_EQ(r_.GetSearchResultType(), ash::SearchResultType::ASSISTANT);
    EXPECT_TRUE(r_.chip_icon().BackedBySameObjectAs(gfx::CreateVectorIcon(
        ash::kAssistantIcon,
        ash::AppListConfig::instance().suggestion_chip_icon_dimension(),
        gfx::kPlaceholderColor)));
  }

  Expect(const Expect&) = delete;
  Expect& operator=(const Expect&) = delete;
  ~Expect() = default;

  Expect& Matches(const AssistantSuggestion& starter) {
    EXPECT_EQ(r_.id(), "googleassistant://" + starter.id.ToString());
    EXPECT_EQ(r_.title(), base::UTF8ToUTF16(starter.text));
    EXPECT_EQ(r_.dismiss_view_on_open(),
              !ash::assistant::util::IsDeepLinkUrl(starter.action_url) &&
                  !starter.action_url.is_empty());
    return *this;
  }

 private:
  const ChromeSearchResult& r_;
};

// ConversationStarterBuilder --------------------------------------------------

class ConversationStarterBuilder {
 public:
  ConversationStarterBuilder() = default;
  ConversationStarterBuilder(const ConversationStarterBuilder&) = delete;
  ConversationStarterBuilder& operator=(const ConversationStarterBuilder&) =
      delete;
  ~ConversationStarterBuilder() = default;

  AssistantSuggestionPtr Build() {
    DCHECK(!id_.is_empty());
    DCHECK(!text_.empty());

    AssistantSuggestionPtr conversation_starter = AssistantSuggestion::New();
    conversation_starter->id = id_;
    conversation_starter->text = text_;
    conversation_starter->action_url = action_url_;
    return conversation_starter;
  }

  ConversationStarterBuilder& WithId(const base::UnguessableToken& id) {
    id_ = id;
    return *this;
  }

  ConversationStarterBuilder& WithText(const std::string& text) {
    text_ = text;
    return *this;
  }

  ConversationStarterBuilder& WithActionUrl(const std::string& action_url) {
    action_url_ = GURL(action_url);
    return *this;
  }

 private:
  base::UnguessableToken id_;
  std::string text_;
  GURL action_url_;
};

// TestAssistantState ----------------------------------------------------------

class TestAssistantState : public ash::AssistantState {
 public:
  TestAssistantState() {
    allowed_state_ = chromeos::assistant::AssistantAllowedState::ALLOWED;
    settings_enabled_ = true;
  }

  TestAssistantState(const TestAssistantState&) = delete;
  TestAssistantState& operator=(const TestAssistantState&) = delete;
  ~TestAssistantState() override = default;

  void SetAllowedState(AssistantAllowedState allowed_state) {
    if (allowed_state_ != allowed_state) {
      allowed_state_ = allowed_state;
      for (auto& observer : observers_)
        observer.OnAssistantFeatureAllowedChanged(allowed_state_.value());
    }
  }

  void SetSettingsEnabled(bool enabled) {
    if (settings_enabled_ != enabled) {
      settings_enabled_ = enabled;
      for (auto& observer : observers_)
        observer.OnAssistantSettingsEnabled(settings_enabled_.value());
    }
  }
};

// TestAssistantSuggestionsController ------------------------------------------

class TestAssistantSuggestionsController
    : public ash::AssistantSuggestionsController {
 public:
  TestAssistantSuggestionsController() {
    SetConversationStarter(ConversationStarterBuilder()
                               .WithId(base::UnguessableToken::Create())
                               .WithText("Initial result")
                               .Build());
  }

  TestAssistantSuggestionsController(
      const TestAssistantSuggestionsController&) = delete;
  TestAssistantSuggestionsController& operator=(
      const TestAssistantSuggestionsController&) = delete;
  ~TestAssistantSuggestionsController() override = default;

  // ash::AssistantSuggestionsController:
  const ash::AssistantSuggestionsModel* GetModel() const override {
    return &model_;
  }

  void AddModelObserver(
      ash::AssistantSuggestionsModelObserver* observer) override {
    model_.AddObserver(observer);
  }

  void RemoveModelObserver(
      ash::AssistantSuggestionsModelObserver* observer) override {
    model_.RemoveObserver(observer);
  }

  void ClearConversationStarters() { SetConversationStarters({}); }

  void SetConversationStarter(AssistantSuggestionPtr conversation_starter) {
    std::vector<AssistantSuggestionPtr> conversation_starters;
    conversation_starters.push_back(std::move(conversation_starter));
    SetConversationStarters(std::move(conversation_starters));
  }

  void SetConversationStarters(
      std::vector<AssistantSuggestionPtr> conversation_starters) {
    model_.SetConversationStarters(std::move(conversation_starters));
  }

 private:
  ash::AssistantSuggestionsModel model_;
};

// AssistantSearchProviderTest -------------------------------------------------

class AssistantSearchProviderTest : public AppListTestBase {
 public:
  AssistantSearchProviderTest() = default;
  AssistantSearchProviderTest(const AssistantSearchProviderTest&) = delete;
  AssistantSearchProviderTest& operator=(const AssistantSearchProviderTest&) =
      delete;
  ~AssistantSearchProviderTest() override = default;

  AssistantSearchProvider& search_provider() { return search_provider_; }

  TestAssistantState& assistant_state() { return assistant_state_; }

  NiceMock<ash::MockAssistantController>& assistant_controller() {
    return assistant_controller_;
  }

  TestAssistantSuggestionsController& suggestions_controller() {
    return suggestions_controller_;
  }

 private:
  TestAssistantState assistant_state_;
  NiceMock<ash::MockAssistantController> assistant_controller_;
  TestAssistantSuggestionsController suggestions_controller_;
  AssistantSearchProvider search_provider_;
};

// Tests -----------------------------------------------------------------------

TEST_F(AssistantSearchProviderTest, ShouldHaveAnInitialResult) {
  std::vector<const AssistantSuggestion*> conversation_starters =
      suggestions_controller().GetModel()->GetConversationStarters();

  ASSERT_EQ(1u, conversation_starters.size());
  ASSERT_EQ(1u, search_provider().results().size());

  const ChromeSearchResult& result = *search_provider().results().at(0);
  Expect(result).Matches(*conversation_starters.front());
}

TEST_F(AssistantSearchProviderTest,
       ShouldUpdateResultsWhenAssistantAllowedStateChanges) {
  // We default to Assistant allowed, so we should have an initial result.
  EXPECT_EQ(1u, search_provider().results().size());

  // Test all possible Assistant allowed states.
  for (int i = 0; i < static_cast<int>(AssistantAllowedState::MAX_VALUE); ++i) {
    if (i == static_cast<int>(AssistantAllowedState::ALLOWED))
      continue;

    // When Assistant becomes not-allowed, results should be cleared.
    assistant_state().SetAllowedState(static_cast<AssistantAllowedState>(i));
    EXPECT_TRUE(search_provider().results().empty());

    // When Assistant becomes allowed, we should again have a single result.
    assistant_state().SetAllowedState(AssistantAllowedState::ALLOWED);
    EXPECT_EQ(1u, search_provider().results().size());
  }
}

TEST_F(AssistantSearchProviderTest,
       ShouldUpdateResultsWhenAssistantSettingsEnabledChanges) {
  // We default to Assistant enabled, so we should have an initial result.
  ASSERT_EQ(1u, search_provider().results().size());

  // When Assistant is disabled, results should be cleared.
  assistant_state().SetSettingsEnabled(false);
  EXPECT_TRUE(search_provider().results().empty());

  // When Assistant is enabled, we should again have a single result.
  assistant_state().SetSettingsEnabled(true);
  ASSERT_EQ(1u, search_provider().results().size());
}

TEST_F(AssistantSearchProviderTest,
       ShouldClearResultsWhenConversationStartersChange) {
  EXPECT_EQ(1u, search_provider().results().size());

  suggestions_controller().ClearConversationStarters();
  EXPECT_TRUE(search_provider().results().empty());
}

TEST_F(AssistantSearchProviderTest,
       ShouldUpdateResultsWhenConversationStartersChange) {
  AssistantSuggestionPtr update = ConversationStarterBuilder()
                                      .WithId(base::UnguessableToken::Create())
                                      .WithText("Updated result")
                                      .Build();

  suggestions_controller().SetConversationStarter(update->Clone());
  ASSERT_EQ(1u, search_provider().results().size());

  const ChromeSearchResult& result = *search_provider().results().at(0);
  Expect(result).Matches(*update);
}

TEST_F(AssistantSearchProviderTest,
       ShouldDelegateOpeningResultsToAssistantController) {
  std::vector<std::pair<AssistantSuggestionPtr, GURL>> test_cases;

  // Test case 1:
  // Action URLs which are *not* Assistant deep links should *not* be modified.
  test_cases.push_back(
      std::make_pair(ConversationStarterBuilder()
                         .WithId(base::UnguessableToken::Create())
                         .WithText("Search")
                         .WithActionUrl("https://www.google.com/search")
                         .Build(),
                     /*expected_url=*/GURL("https://www.google.com/search")));

  // Test case 2:
  // We expect Assistant deep links to accurately reflect launcher chip as being
  // both the entry point into Assistant UI as well as the query source.
  test_cases.push_back(std::make_pair(
      ConversationStarterBuilder()
          .WithId(base::UnguessableToken::Create())
          .WithText("Weather")
          .WithActionUrl("googleassistant://send-query?q=weather")
          .Build(),
      /*expected_url=*/GURL(base::StringPrintf(
          "googleassistant://send-query?q=weather&entryPoint=%d&querySource=%d",
          static_cast<int>(AssistantEntryPoint::kLauncherChip),
          static_cast<int>(AssistantQuerySource::kLauncherChip)))));

  // Test case 3:
  // When conversation starters do *not* specify an action URL explicitly, it is
  // implicitly understood that they should trigger an Assistant query composed
  // of their display text.
  test_cases.push_back(std::make_pair(
      ConversationStarterBuilder()
          .WithId(base::UnguessableToken::Create())
          .WithText("What can you do?")
          .Build(),
      /*expected_url=*/GURL(base::StringPrintf(
          "googleassistant://"
          "send-query?q=What+can+you+do%%3F&entryPoint=%d&querySource=%d",
          static_cast<int>(AssistantEntryPoint::kLauncherChip),
          static_cast<int>(AssistantQuerySource::kLauncherChip)))));

  for (auto& test_case : test_cases) {
    suggestions_controller().SetConversationStarter(std::move(test_case.first));
    ASSERT_EQ(1u, search_provider().results().size());

    GURL url;
    bool in_background = true;
    bool from_user = true;
    EXPECT_CALL(assistant_controller(), OpenUrl)
        .WillOnce(DoAll(SaveArg<0>(&url), SaveArg<1>(&in_background),
                        SaveArg<2>(&from_user)));

    search_provider().results().at(0)->Open(/*event_flags=*/0);

    EXPECT_EQ(/*expected_url=*/test_case.second, url);
    EXPECT_FALSE(in_background);
    EXPECT_FALSE(from_user);
  }
}

}  // namespace test
}  // namespace app_list
