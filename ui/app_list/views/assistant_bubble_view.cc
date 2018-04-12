// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/assistant_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/assistant_interaction_model.h"
#include "ui/app_list/views/suggestion_chip_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;
constexpr int kCornerRadiusDip = 16;
constexpr int kPaddingDip = 8;
constexpr int kPreferredWidthDip = 364;
constexpr int kSpacingDip = 8;

// Typography.
constexpr SkColor kTextColorPrimary = SkColorSetA(SK_ColorBLACK, 0xDE);

// TODO(dmblack): Remove after removing placeholders.
// Placeholder.
constexpr int kPlaceholderCardPreferredHeightDip = 200;
constexpr int kPlaceholderCardCornerRadiusDip = 4;
constexpr SkColor kPlaceholderColor = SkColorSetA(SK_ColorBLACK, 0x1F);
constexpr int kPlaceholderIconSizeDip = 32;

// TODO(b/77638210): Replace with localized resource string.
constexpr char kPlaceholderPrompt[] = "Hi, how can I help?";

// TODO(dmblack): Remove after removing placeholders.
// RoundRectBackground ---------------------------------------------------------

class RoundRectBackground : public views::Background {
 public:
  RoundRectBackground(SkColor color, int corner_radius)
      : color_(color), corner_radius_(corner_radius) {}

  ~RoundRectBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    canvas->DrawRoundRect(view->GetContentsBounds(), corner_radius_, flags);
  }

 private:
  const SkColor color_;
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundRectBackground);
};

// InteractionContainer --------------------------------------------------------

class InteractionContainer : public views::View {
 public:
  InteractionContainer() : interaction_label_(new views::Label()) {
    InitLayout();
  }

  ~InteractionContainer() override = default;

  void SetRecognizedSpeech(const RecognizedSpeech& recognized_speech) {
    // TODO(dmblack): Represent high confidence and low confidence portions of
    // the recognized speech with different colors.
    interaction_label_->SetText(
        base::UTF8ToUTF16(recognized_speech.high_confidence_text) +
        base::UTF8ToUTF16(recognized_speech.low_confidence_text));

    PreferredSizeChanged();
  }

  void ClearRecognizedSpeech() {
    interaction_label_->SetText(base::ASCIIToUTF16(kPlaceholderPrompt));

    PreferredSizeChanged();
  }

 private:
  void InitLayout() {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(0, kPaddingDip), kSpacingDip));

    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

    // TODO(dmblack): Implement stateful icon. Icon will change state in
    // correlation with speech recognition events.
    // Icon placeholder.
    views::View* icon_placeholder = new views::View();
    icon_placeholder->SetBackground(std::make_unique<RoundRectBackground>(
        kPlaceholderColor, kPlaceholderIconSizeDip / 2));
    icon_placeholder->SetPreferredSize(
        gfx::Size(kPlaceholderIconSizeDip, kPlaceholderIconSizeDip));
    AddChildView(icon_placeholder);

    // Interaction label.
    interaction_label_->SetAutoColorReadabilityEnabled(false);
    interaction_label_->SetEnabledColor(kTextColorPrimary);
    interaction_label_->SetFontList(
        interaction_label_->font_list().DeriveWithSizeDelta(4));
    interaction_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    interaction_label_->SetMultiLine(true);
    interaction_label_->SetText(base::ASCIIToUTF16(kPlaceholderPrompt));
    AddChildView(interaction_label_);

    layout->SetFlexForView(interaction_label_, 1);
  }

  views::Label* interaction_label_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(InteractionContainer);
};

// TextContainer ---------------------------------------------------------------

class TextContainer : public views::View {
 public:
  TextContainer() { InitLayout(); }

  ~TextContainer() override = default;

  void AddText(const std::string& text) {
    views::Label* text_view = new views::Label(base::UTF8ToUTF16(text));
    text_view->SetAutoColorReadabilityEnabled(false);
    text_view->SetEnabledColor(kTextColorPrimary);
    text_view->SetFontList(text_view->font_list().DeriveWithSizeDelta(4));
    text_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    text_view->SetMultiLine(true);
    AddChildView(text_view);

    PreferredSizeChanged();
  }

  void ClearText() {
    RemoveAllChildViews(true);
    PreferredSizeChanged();
  }

 private:
  void InitLayout() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(0, kPaddingDip),
        kSpacingDip));
  }

  DISALLOW_COPY_AND_ASSIGN(TextContainer);
};

// TODO(dmblack): Container should wrap chips in a horizontal scroll view.
// SuggestionsContainer --------------------------------------------------------

class SuggestionsContainer : public views::View {
 public:
  SuggestionsContainer() = default;

  ~SuggestionsContainer() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    int width = 0;
    int height = 0;

    for (int i = 0; i < child_count(); i++) {
      const views::View* child = child_at(i);
      gfx::Size child_size = child->GetPreferredSize();

      if (i > 0) {
        // Add spacing between chips.
        width += kSpacingDip;
      }

      width += child_size.width();
      height = std::max(child_size.height(), height);
    }

    if (width > 0) {
      // Add horizontal padding.
      width += 2 * kPaddingDip;
    }

    return gfx::Size(width, height);
  }

  // views::View:
  void Layout() override {
    const int height = views::View::height();
    int left = kPaddingDip;

    for (int i = 0; i < child_count(); i++) {
      views::View* child = child_at(i);
      gfx::Size child_size = child->GetPreferredSize();

      child->SetBounds(left, (height - child_size.height()) / 2,
                       child_size.width(), child_size.height());

      left += child_size.width() + kSpacingDip;
    }
  }

  void AddSuggestions(const std::vector<std::string>& suggestions) {
    for (const std::string& suggestion : suggestions) {
      AddChildView(new SuggestionChipView(base::UTF8ToUTF16(suggestion)));
    }
    PreferredSizeChanged();
  }

  void ClearSuggestions() {
    RemoveAllChildViews(true);
    PreferredSizeChanged();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestionsContainer);
};

}  // namespace

// AssistantBubbleView ---------------------------------------------------------

AssistantBubbleView::AssistantBubbleView(
    AssistantInteractionModel* assistant_interaction_model)
    : assistant_interaction_model_(assistant_interaction_model),
      interaction_container_(new InteractionContainer()),
      text_container_(new TextContainer()),
      card_container_(new views::View()),
      suggestions_container_(new SuggestionsContainer()) {
  InitLayout();

  // Observe changes to interaction model.
  DCHECK(assistant_interaction_model_);
  assistant_interaction_model_->AddObserver(this);
}

AssistantBubbleView::~AssistantBubbleView() {
  assistant_interaction_model_->RemoveObserver(this);
}

gfx::Size AssistantBubbleView::CalculatePreferredSize() const {
  int preferred_height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kPreferredWidthDip);
  return gfx::Size(kPreferredWidthDip, preferred_height);
}

void AssistantBubbleView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantBubbleView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantBubbleView::InitLayout() {
  SetBackground(std::make_unique<RoundRectBackground>(kBackgroundColor,
                                                      kCornerRadiusDip));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kPaddingDip, 0),
      kSpacingDip));

  // Interaction container.
  AddChildView(interaction_container_);

  // Text container.
  text_container_->SetVisible(false);
  AddChildView(text_container_);

  // Card container.
  card_container_->SetVisible(false);
  card_container_->SetBackground(std::make_unique<RoundRectBackground>(
      kPlaceholderColor, kPlaceholderCardCornerRadiusDip));
  card_container_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(0, kPaddingDip)));
  card_container_->SetPreferredSize(
      gfx::Size(INT_MAX, kPlaceholderCardPreferredHeightDip));
  AddChildView(card_container_);

  // Suggestions container.
  suggestions_container_->SetVisible(false);
  AddChildView(suggestions_container_);
}

void AssistantBubbleView::OnCardChanged(const std::string& html) {
  // TODO(dmblack): Handle HTML.
  card_container_->SetVisible(true);
}

void AssistantBubbleView::OnCardCleared() {
  card_container_->SetVisible(false);
}

void AssistantBubbleView::OnRecognizedSpeechChanged(
    const RecognizedSpeech& recognized_speech) {
  interaction_container_->SetRecognizedSpeech(recognized_speech);
}

void AssistantBubbleView::OnRecognizedSpeechCleared() {
  interaction_container_->ClearRecognizedSpeech();
}

void AssistantBubbleView::OnSuggestionsAdded(
    const std::vector<std::string>& suggestions) {
  suggestions_container_->AddSuggestions(suggestions);
  suggestions_container_->SetVisible(true);
}

void AssistantBubbleView::OnSuggestionsCleared() {
  suggestions_container_->ClearSuggestions();
  suggestions_container_->SetVisible(false);
}

void AssistantBubbleView::OnTextAdded(const std::string& text) {
  text_container_->AddText(text);
  text_container_->SetVisible(true);
}

void AssistantBubbleView::OnTextCleared() {
  text_container_->ClearText();
  text_container_->SetVisible(false);
}

}  // namespace app_list
