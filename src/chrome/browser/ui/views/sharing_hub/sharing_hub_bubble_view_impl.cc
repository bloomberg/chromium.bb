// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sharing_hub/preview_view.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace sharing_hub {

namespace {

// The valid action button height.
constexpr int kActionButtonHeight = 56;
// Maximum number of buttons that are shown without scrolling. If the number of
// actions is larger than kMaximumButtons, the bubble will be scrollable.
constexpr int kMaximumButtons = 10;

// Used to group the action buttons together, which makes moving between them
// with arrow keys possible.
constexpr int kActionButtonGroup = 0;

}  // namespace

SharingHubBubbleViewImpl::SharingHubBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SharingHubBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller->AsWeakPtr()) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetEnableArrowKeyTraversal(true);
  DCHECK(controller);
}

SharingHubBubbleViewImpl::~SharingHubBubbleViewImpl() = default;

void SharingHubBubbleViewImpl::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

bool SharingHubBubbleViewImpl::ShouldShowCloseButton() const {
  return false;
}

bool SharingHubBubbleViewImpl::ShouldShowWindowTitle() const {
  return false;
}

void SharingHubBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

std::u16string SharingHubBubbleViewImpl::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TOOLTIP);
}

void SharingHubBubbleViewImpl::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
  if (show_time_) {
    share::RecordSharingHubTimeToShow(base::Time::Now() - *show_time_);
    show_time_ = absl::nullopt;
  }
}

void SharingHubBubbleViewImpl::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  if (GetWidget()) {
    set_color(GetColorProvider()->GetColor(ui::kColorMenuBackground));
    share_link_label_->SetBackgroundColor(
        GetColorProvider()->GetColor(ui::kColorMenuBackground));
    share_link_label_->SetEnabledColor(
        GetColorProvider()->GetColor(ui::kColorMenuItemForeground));
  }
}

void SharingHubBubbleViewImpl::Show(DisplayReason reason) {
  show_time_ = base::Time::Now();
  ShowForReason(reason);
}

void SharingHubBubbleViewImpl::OnActionSelected(
    SharingHubBubbleActionButton* button) {
  if (!controller_)
    return;

  controller_->OnActionSelected(button->action_command_id(),
                                button->action_is_first_party(),
                                button->action_name_for_metrics());

  Hide();
}

const views::View* SharingHubBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SharingHubBubbleViewImpl::Init() {
  const int kPadding = 8;
  set_margins(gfx::Insets::TLBR(kPadding, 0, kPadding, 0));
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  if (controller_->ShouldUsePreview()) {
    auto* preview = AddChildView(std::make_unique<PreviewView>(
        controller_->GetPreviewTitle(), controller_->GetPreviewUrl(),
        controller_->GetPreviewImage()));
    preview->TakeCallbackSubscription(
        controller_->RegisterPreviewImageChangedCallback(base::BindRepeating(
            &PreviewView::OnImageChanged, base::Unretained(preview))));
    AddChildView(std::make_unique<views::Separator>());
  }

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kActionButtonHeight * kMaximumButtons);
  scroll_view_->SetBackgroundThemeColorId(ui::kColorMenuBackground);

  PopulateScrollView(controller_->GetFirstPartyActions(),
                     controller_->GetThirdPartyActions());
}

void SharingHubBubbleViewImpl::PopulateScrollView(
    const std::vector<SharingHubAction>& first_party_actions,
    const std::vector<SharingHubAction>& third_party_actions) {
  auto* action_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  action_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  for (const auto& action : first_party_actions) {
    auto* view = action_list_view->AddChildView(
        std::make_unique<SharingHubBubbleActionButton>(this, action));
    view->SetGroup(kActionButtonGroup);
  }

  auto* separator =
      action_list_view->AddChildView(std::make_unique<views::Separator>());
  constexpr int kIndent = 12;
  constexpr int kPadding = 4;
  separator->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(kPadding, 0, kPadding, 0)));

  constexpr int kLabelLineHeight = 32;

  auto* share_link_label =
      new views::Label(l10n_util::GetStringUTF16(IDS_SHARING_HUB_SHARE_LABEL),
                       views::style::CONTEXT_DIALOG_TITLE);
  share_link_label->SetLineHeight(kLabelLineHeight);
  share_link_label->SetMultiLine(true);
  share_link_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  share_link_label->SizeToFit(views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  share_link_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, kIndent, 0, 0)));
  share_link_label_ = action_list_view->AddChildView(share_link_label);

  for (const auto& action : third_party_actions) {
    auto* view = action_list_view->AddChildView(
        std::make_unique<SharingHubBubbleActionButton>(this, action));
    view->SetGroup(kActionButtonGroup);
  }

  MaybeSizeToContents();
  Layout();
}

void SharingHubBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

}  // namespace sharing_hub
