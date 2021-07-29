// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_quiet_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

namespace {

const gfx::VectorIcon& GetPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  if (delegate->Requests()[0]->GetRequestType() ==
      permissions::RequestType::kNotifications) {
    return vector_icons::kNotificationsOffIcon;
  }

  NOTREACHED();
  return gfx::kNoneIcon;
}

std::u16string GetPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  if (delegate->Requests()[0]->GetRequestType() ==
      permissions::RequestType::kNotifications) {
    return l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
  }

  NOTREACHED();
  return std::u16string();
}

}  // namespace

PermissionQuietChip::PermissionQuietChip(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate,
    bool should_expand)
    : PermissionChip(
          delegate,
          {GetPermissionIconId(delegate), GetPermissionMessage(delegate), false,
           /*is_prominent=*/false, OmniboxChipButton::Theme::kGray,
           /*should_expand=*/should_expand}),
      browser_(browser) {
  DCHECK_EQ(1u, delegate->Requests().size());
  chip_shown_time_ = base::TimeTicks::Now();
}

PermissionQuietChip::~PermissionQuietChip() {
  if (quiet_request_bubble_) {
    views::Widget* widget = quiet_request_bubble_->GetWidget();
    widget->RemoveObserver(this);
    widget->Close();
  }
}

void PermissionQuietChip::OpenBubble() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!quiet_request_bubble_);

  LocationBarView* lbv = GetLocationBarView();
  content::WebContents* web_contents = lbv->GetContentSettingWebContents();

  if (web_contents) {
    quiet_request_bubble_ = new ContentSettingBubbleContents(
        std::make_unique<ContentSettingNotificationsBubbleModel>(
            lbv->GetContentSettingBubbleModelDelegate(), web_contents),
        web_contents, lbv, views::BubbleBorder::TOP_LEFT);
    quiet_request_bubble_->SetHighlightedButton(button());
    views::Widget* bubble_widget =
        views::BubbleDialogDelegateView::CreateBubble(quiet_request_bubble_);
    bubble_widget->AddObserver(this);
    bubble_widget->Show();
  }

  RecordChipButtonPressed();
}

views::BubbleDialogDelegateView*
PermissionQuietChip::GetPermissionPromptBubbleForTest() {
  return quiet_request_bubble_;
}

void PermissionQuietChip::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, quiet_request_bubble_->GetWidget());
  PermissionChip::OnWidgetClosing(widget);
  quiet_request_bubble_ = nullptr;
}

bool PermissionQuietChip::IsBubbleShowing() const {
  return quiet_request_bubble_;
}

void PermissionQuietChip::RecordChipButtonPressed() {
  base::UmaHistogramMediumTimes("Permissions.QuietChip.TimeToInteraction",
                                base::TimeTicks::Now() - chip_shown_time_);
}

LocationBarView* PermissionQuietChip::GetLocationBarView() {
  return BrowserView::GetBrowserViewForBrowser(browser_)->GetLocationBarView();
}

BEGIN_METADATA(PermissionQuietChip, views::View)
END_METADATA
