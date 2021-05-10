// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/tip_marquee_view.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/resources_util.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr char kTipMarqueeViewSeparator[] = " - ";

// TODO(crbug.com/1171654): move to localized strings when out of tech demo mode
constexpr char kTipMarqueeViewGotIt[] = "Got it";
constexpr char kTipMarqueeViewClickToHideTip[] = "Click to hide tip";
constexpr char kTipMarqueeViewClickToShowTip[] = "Click to show tip";
constexpr char kTipMarqueeViewClickToLearnMore[] = "Click to learn more";

// ------------------------------------------------------------------
// TODO(crbug.com/1171654): remove the entire section below before this code
// ships, once the UX demo is over.
//
// This section provides the ability to show a demo tip marquee on startup with
// placeholder text, for UX evaluation only. This code should be removed before
// beta roll.
//
// Valid command-line arguments are:
//  --tip-marquee-view-test=simple
//      Displays a tip with no "learn more" link
//  --tip-marquee-view-test=learn-more
//      Displays a tip with a "learn more" link that displays a sample bubble
//
constexpr char kTipMarqueeViewTestSwitch[] = "tip-marquee-view-test";
constexpr char kTipMarqueeViewTestTypeSimple[] = "simple";
constexpr char kTipMarqueeViewTestTypeLearnMore[] = "learn-more";
constexpr char kTipMarqueeViewTestTitleText[] = "Lorem Ipsum";
constexpr char kTipMarqueeViewTestText[] =
    "Lorem ipsum dolor sit amet consectetur";
constexpr char kTipMarqueeViewTestBodyText[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat.";

class TestTipMarqueeViewLearnMoreBubble
    : public views::BubbleDialogDelegateView {
 public:
  explicit TestTipMarqueeViewLearnMoreBubble(TipMarqueeView* marquee)
      : BubbleDialogDelegateView(marquee, views::BubbleBorder::TOP_LEFT),
        marquee_(marquee) {
    SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   base::ASCIIToUTF16(kTipMarqueeViewGotIt));
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(IDS_CLOSE));
    SetAcceptCallback(base::BindOnce(
        &TestTipMarqueeViewLearnMoreBubble::OnAccept, base::Unretained(this)));
    set_close_on_deactivate(true);
    SetOwnedByWidget(true);

    auto* const layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    layout->SetInteriorMargin(gfx::Insets(10, 0, 0, 0));
    views::View* const placeholder_image =
        AddChildView(std::make_unique<views::View>());
    placeholder_image->SetPreferredSize(gfx::Size(150, 175));
    placeholder_image->SetBackground(
        views::CreateSolidBackground(SK_ColorLTGRAY));

    views::View* const rhs_view = AddChildView(std::make_unique<views::View>());
    auto* const rhs_layout =
        rhs_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
    rhs_layout->SetOrientation(views::LayoutOrientation::kVertical);
    rhs_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    rhs_layout->SetInteriorMargin(gfx::Insets(0, 16, 0, 0));

    auto* const title_text =
        rhs_view->AddChildView(std::make_unique<views::Label>(
            base::ASCIIToUTF16(kTipMarqueeViewTestTitleText),
            views::style::CONTEXT_DIALOG_TITLE));
    title_text->SetProperty(views::kMarginsKey, gfx::Insets(6, 0, 10, 0));

    auto* const body_text =
        rhs_view->AddChildView(std::make_unique<views::Label>(
            base::ASCIIToUTF16(kTipMarqueeViewTestBodyText),
            views::style::CONTEXT_DIALOG_BODY_TEXT));
    body_text->SetMultiLine(true);
    body_text->SetMaximumWidth(250);
    body_text->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  }

 private:
  void OnAccept() { marquee_->ClearTip(); }

  TipMarqueeView* const marquee_;
};

void ShowTestTipMarqueeViewLearnMoreBubble(TipMarqueeView* marquee) {
  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<TestTipMarqueeViewLearnMoreBubble>(marquee))
      ->Show();
}

void MaybeShowTestTipMarqueeView(TipMarqueeView* marquee) {
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTipMarqueeViewTestSwitch)) {
    const std::string test_type =
        command_line->GetSwitchValueASCII(kTipMarqueeViewTestSwitch);
    if (test_type == kTipMarqueeViewTestTypeSimple) {
      marquee->SetTip(base::ASCIIToUTF16(kTipMarqueeViewTestText));
    } else if (test_type == kTipMarqueeViewTestTypeLearnMore) {
      marquee->SetTip(
          base::ASCIIToUTF16(kTipMarqueeViewTestText),
          base::BindRepeating(&ShowTestTipMarqueeViewLearnMoreBubble));
    } else {
      LOG(WARNING) << "Invalid switch value: --" << kTipMarqueeViewTestSwitch
                   << "=" << test_type;
    }
  }
}

// TODO(crbug.com/1171654): remove the entire section above before this code
// ships, once the UX demo is over.
// ------------------------------------------------------------------

// The width of the multiline text display in the overflow bubble.
constexpr int kTipMarqueeViewOverflowTextWidth = 250;

class TipMarqueeOverflowBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(TipMarqueeOverflowBubbleView);
  TipMarqueeOverflowBubbleView(TipMarqueeView* tip_marquee_view,
                               const base::string16& text)
      : BubbleDialogDelegateView(tip_marquee_view,
                                 views::BubbleBorder::TOP_LEFT),
        tip_marquee_view_(tip_marquee_view) {
    SetButtons(ui::DIALOG_BUTTON_OK);
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   base::ASCIIToUTF16(kTipMarqueeViewGotIt));
    SetAcceptCallback(base::BindOnce(&TipMarqueeOverflowBubbleView::OnAccept,
                                     base::Unretained(this)));
    set_close_on_deactivate(true);
    SetOwnedByWidget(true);

    auto* const label = AddChildView(std::make_unique<views::Label>(
        text, views::style::CONTEXT_DIALOG_BODY_TEXT));
    label->SetMultiLine(true);
    label->SetMaximumWidth(kTipMarqueeViewOverflowTextWidth);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }
  ~TipMarqueeOverflowBubbleView() override = default;

 private:
  void OnAccept() {
    LOG(WARNING) << "OnAccept()";
    tip_marquee_view_->ClearTip();
  }

  TipMarqueeView* const tip_marquee_view_;
};

BEGIN_METADATA(TipMarqueeOverflowBubbleView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace

constexpr int TipMarqueeView::kTipMarqueeIconSize;
constexpr int TipMarqueeView::kTipMarqueeIconPadding;
constexpr int TipMarqueeView::kTipMarqueeIconTotalWidth;

TipMarqueeView::TipMarqueeView(int text_context, int text_style) {
  tip_text_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  tip_text_label_->SetTextContext(text_context);
  tip_text_label_->SetDefaultTextStyle(text_style);
  // TODO(dfried): Figure out how to set elide behavior.
  // tip_text_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kTipMarqueeIconTotalWidth, 0, 0)));

  SetVisible(false);

  MaybeShowTestTipMarqueeView(this);
}

TipMarqueeView::~TipMarqueeView() = default;

bool TipMarqueeView::SetTip(
    const base::string16& tip_text,
    LearnMoreLinkClickedCallback learn_more_link_clicked_callback) {
  tip_text_ = tip_text;
  base::string16 full_tip = tip_text;
  const base::string16 separator = base::ASCIIToUTF16(kTipMarqueeViewSeparator);
  const size_t tip_text_length = tip_text.length();
  const bool has_learn_more_link = !learn_more_link_clicked_callback.is_null();
  full_tip.append(separator);
  if (has_learn_more_link)
    full_tip.append(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  else
    full_tip.append(base::ASCIIToUTF16(kTipMarqueeViewGotIt));
  tip_text_label_->SetText(full_tip);
  tip_text_label_->AddStyleRange(
      gfx::Range(tip_text_length + separator.length(), full_tip.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &TipMarqueeView::LinkClicked, base::Unretained(this))));
  learn_more_link_clicked_callback_ = learn_more_link_clicked_callback;
  collapsed_ = false;
  SetVisible(true);
  return !collapsed_;
}

void TipMarqueeView::ClearTip() {
  if (show_tip_widget_)
    show_tip_widget_->Close();
  tip_text_label_->SetText(base::string16());
  tip_text_.clear();
  learn_more_link_clicked_callback_.Reset();
  SetVisible(false);
}

bool TipMarqueeView::OnMousePressed(const ui::MouseEvent& event) {
  if (!IsPointInIcon(event.location()))
    return false;
  if (!GetFitsInLayout()) {
    if (learn_more_link_clicked_callback_)
      LinkClicked();
    else
      ToggleOverflowWidget();
  } else {
    collapsed_ = !collapsed_;
    InvalidateLayout();
  }
  return true;
}

gfx::Size TipMarqueeView::CalculatePreferredSize() const {
  if (collapsed_)
    return GetMinimumSize();

  const gfx::Size label_size = tip_text_label_->GetPreferredSize();
  const int width = label_size.width() + kTipMarqueeIconTotalWidth;
  const int height = std::max(label_size.height(), kTipMarqueeIconSize);
  return gfx::Size(width, height);
}

gfx::Size TipMarqueeView::GetMinimumSize() const {
  return gfx::Size(kTipMarqueeIconSize, kTipMarqueeIconSize);
}

void TipMarqueeView::Layout() {
  // TODO(dfried): animate label
  if (collapsed_ || size().width() < GetPreferredSize().width()) {
    tip_text_label_->SetVisible(false);
  } else {
    tip_text_label_->SetVisible(true);
    gfx::Rect text_rect = GetContentsBounds();
    text_rect.Inset(0,
                    std::max(0, (text_rect.height() -
                                 tip_text_label_->GetPreferredSize().height()) /
                                    2));
    tip_text_label_->SetBoundsRect(text_rect);
  }
}

void TipMarqueeView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::ImageSkia* const icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PRODUCT_LOGO_16);
  canvas->DrawImageInt(*icon, 0, 0, kTipMarqueeIconSize, kTipMarqueeIconSize, 0,
                       (height() - kTipMarqueeIconSize) / 2,
                       kTipMarqueeIconSize, kTipMarqueeIconSize, true);
}

base::string16 TipMarqueeView::GetTooltipText(const gfx::Point& p) const {
  if (!IsPointInIcon(p))
    return View::GetTooltipText(p);

  // TODO(pkasting): Localize
  if (tip_text_label_->GetVisible())
    return base::ASCIIToUTF16(kTipMarqueeViewClickToHideTip);
  if (GetFitsInLayout())
    return base::ASCIIToUTF16(kTipMarqueeViewClickToShowTip);
  base::string16 result = tip_text_;
  if (learn_more_link_clicked_callback_) {
    result.append(base::ASCIIToUTF16(kTipMarqueeViewSeparator));
    result.append(base::ASCIIToUTF16(kTipMarqueeViewClickToLearnMore));
  }
  return result;
}

void TipMarqueeView::LinkClicked() {
  if (!learn_more_link_clicked_callback_) {
    ClearTip();
    return;
  }
  if (GetProperty(views::kAnchoredDialogKey))
    return;
  learn_more_link_clicked_callback_.Run(this);
}

bool TipMarqueeView::GetFitsInLayout() const {
  const views::SizeBounds available = parent()->GetAvailableSize(this);
  if (!available.width().is_bounded())
    return true;
  return available.width().value() >=
         tip_text_label_->GetPreferredSize().width() +
             kTipMarqueeIconTotalWidth;
}

bool TipMarqueeView::IsPointInIcon(const gfx::Point& p) const {
  const int pos = GetMirroredXInView(p.x());
  return pos < kTipMarqueeIconTotalWidth;
}

void TipMarqueeView::ToggleOverflowWidget() {
  if (show_tip_widget_) {
    show_tip_widget_->Close();
    return;
  }

  DCHECK(!tip_text_.empty());
  DCHECK(!show_tip_widget_);
  show_tip_widget_ = views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<TipMarqueeOverflowBubbleView>(this, tip_text_));
  widget_observer_.Observe(show_tip_widget_);
  show_tip_widget_->Show();
}

void TipMarqueeView::OnWidgetDestroying(views::Widget* widget) {
  widget_observer_.Reset();
  if (widget != show_tip_widget_)
    return;
  show_tip_widget_ = nullptr;
}

BEGIN_METADATA(TipMarqueeView, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, FitsInLayout)
END_METADATA
