// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

using chromeos::assistant::AssistantSuggestion;
using chromeos::assistant::AssistantSuggestionType;

// Greeting.
constexpr int kGreetingLabelLineHeight = 28;
constexpr int kGreetingLabelSizeDelta = 10;

// Intro.
constexpr int kIntroLabelLineHeight = 20;
constexpr int kIntroLabelMarginTopDip = 12;
constexpr int kIntroLabelSizeDelta = 2;

// Suggestions.
constexpr int kSuggestionsColumnCount = 3;
constexpr int kSuggestionsColumnSetId = 1;
constexpr int kSuggestionsMaxCount = 6;
constexpr int kSuggestionsMarginDip = 16;
constexpr int kSuggestionsMarginTopDip = 32;

// Helpers ---------------------------------------------------------------------

std::string GetGreetingMessage(AssistantViewDelegate* delegate) {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  const std::string given_name = delegate->GetPrimaryUserGivenName();

  if (now.hour < 5) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT,
                     base::UTF8ToUTF16(given_name));
  }

  if (now.hour < 12) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_MORNING_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_MORNING,
                     base::UTF8ToUTF16(given_name));
  }

  if (now.hour < 17) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_AFTERNOON_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_AFTERNOON,
                     base::UTF8ToUTF16(given_name));
  }

  if (now.hour < 23) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_EVENING_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_EVENING,
                     base::UTF8ToUTF16(given_name));
  }

  return given_name.empty()
             ? l10n_util::GetStringUTF8(
                   IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT_WITHOUT_NAME)
             : l10n_util::GetStringFUTF8(
                   IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT,
                   base::UTF8ToUTF16(given_name));
}

}  // namespace

// AssistantOnboardingView -----------------------------------------------------

AssistantOnboardingView::AssistantOnboardingView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kOnboardingView);
  InitLayout();

  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantSuggestionsController::Get()->GetModel()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantOnboardingView::~AssistantOnboardingView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);

  if (AssistantSuggestionsController::Get())
    AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
}

const char* AssistantOnboardingView::GetClassName() const {
  return "AssistantOnboardingView";
}

gfx::Size AssistantOnboardingView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantOnboardingView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantOnboardingView::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void AssistantOnboardingView::OnOnboardingSuggestionsChanged(
    const std::vector<AssistantSuggestion>& onboarding_suggestions) {
  UpdateSuggestions();
}

void AssistantOnboardingView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    absl::optional<AssistantEntryPoint> entry_point,
    absl::optional<AssistantExitPoint> exit_point) {
  if (new_visibility != AssistantVisibility::kVisible)
    return;

  UpdateGreeting();

  if (IsDrawn())
    delegate_->OnOnboardingShown();
}

void AssistantOnboardingView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kHorizontalMarginDip)));

  // Greeting.
  greeting_ = AddChildView(std::make_unique<views::Label>());
  greeting_->SetAutoColorReadabilityEnabled(false);
  greeting_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  greeting_->SetEnabledColor(kTextColorPrimary);
  greeting_->SetFontList(assistant::ui::GetDefaultFontList()
                             .DeriveWithSizeDelta(kGreetingLabelSizeDelta)
                             .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  greeting_->SetLineHeight(kGreetingLabelLineHeight);
  greeting_->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));

  // Intro.
  auto intro = std::make_unique<views::Label>();
  intro->SetAutoColorReadabilityEnabled(false);
  intro->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  intro->SetBorder(views::CreateEmptyBorder(kIntroLabelMarginTopDip, 0, 0, 0));
  intro->SetEnabledColor(kTextColorPrimary);
  intro->SetFontList(assistant::ui::GetDefaultFontList()
                         .DeriveWithSizeDelta(kIntroLabelSizeDelta)
                         .DeriveWithWeight(gfx::Font ::Weight::MEDIUM));
  intro->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  intro->SetLineHeight(kIntroLabelLineHeight);
  intro->SetMultiLine(true);
  intro->SetText(
      l10n_util::GetStringUTF16(IDS_ASSISTANT_BETTER_ONBOARDING_INTRO));
  AddChildView(std::move(intro));

  // Suggestions.
  UpdateSuggestions();
}

void AssistantOnboardingView::UpdateSuggestions() {
  if (grid_)
    RemoveChildViewT(grid_);

  grid_ = AddChildView(std::make_unique<views::View>());
  grid_->SetBorder(views::CreateEmptyBorder(kSuggestionsMarginTopDip, 0, 0, 0));

  auto* layout = grid_->SetLayoutManager(std::make_unique<views::GridLayout>());
  auto* columns = layout->AddColumnSet(kSuggestionsColumnSetId);

  // Initialize columns.
  for (int i = 0; i < kSuggestionsColumnCount; ++i) {
    if (i > 0) {
      columns->AddPaddingColumn(
          /*resize_percent=*/views::GridLayout::kFixedSize,
          /*width=*/kSuggestionsMarginDip);
    }
    columns->AddColumn(
        /*h_align=*/views::GridLayout::Alignment::FILL,
        /*v_align=*/views::GridLayout::Alignment::FILL, /*resize_percent=*/1.0,
        /*size_type=*/views::GridLayout::ColumnSize::kFixed,
        /*fixed_width=*/0, /*min_width=*/0);
  }

  const std::vector<AssistantSuggestion>& suggestions =
      AssistantSuggestionsController::Get()
          ->GetModel()
          ->GetOnboardingSuggestions();

  // Initialize suggestions.
  for (size_t i = 0; i < suggestions.size() && i < kSuggestionsMaxCount; ++i) {
    if (i % kSuggestionsColumnCount == 0) {
      if (i > 0) {
        layout->StartRowWithPadding(
            /*vertical_resize=*/views::GridLayout::kFixedSize,
            /*column_set_id=*/kSuggestionsColumnSetId,
            /*padding_resize=*/views::GridLayout::kFixedSize,
            /*padding=*/kSuggestionsMarginDip);
      } else {
        layout->StartRow(/*vertical_resize=*/views::GridLayout::kFixedSize,
                         /*column_set_id=*/kSuggestionsColumnSetId);
      }
    }
    layout->AddView(std::make_unique<AssistantOnboardingSuggestionView>(
        delegate_, suggestions.at(i), i));
  }
}

void AssistantOnboardingView::UpdateGreeting() {
  greeting_->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));
}

}  // namespace ash
