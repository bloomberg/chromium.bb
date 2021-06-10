// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
bool IsCameraPermission(permissions::RequestType type) {
  return type == permissions::RequestType::kCameraStream;
}

bool IsCameraOrMicPermission(permissions::RequestType type) {
  return type == permissions::RequestType::kCameraStream ||
         type == permissions::RequestType::kMicStream;
}

const gfx::VectorIcon& GetPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  auto requests = delegate->Requests();
  if (requests.size() == 1)
    return permissions::GetIconId(requests[0]->GetRequestType());

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->GetRequestType())
             ? permissions::GetIconId(requests[0]->GetRequestType())
             : permissions::GetIconId(requests[1]->GetRequestType());
}

std::u16string GetPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  auto requests = delegate->Requests();

  return requests.size() == 1
             ? requests[0]->GetChipText().value()
             : l10n_util::GetStringUTF16(
                   IDS_MEDIA_CAPTURE_VIDEO_AND_AUDIO_PERMISSION_CHIP);
}

void VerifyCameraAndMicRequest(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  const std::vector<permissions::PermissionRequest*>& requests =
      delegate->Requests();

  // TODO(olesiamarukhno): Add combined camera & microphone permission and
  // update delegate to contain only one request at a time.
  DCHECK(requests.size() == 1u || requests.size() == 2u);
  if (requests.size() == 2) {
    DCHECK(IsCameraOrMicPermission(requests[0]->GetRequestType()));
    DCHECK(IsCameraOrMicPermission(requests[1]->GetRequestType()));
    DCHECK_NE(requests[0]->GetRequestType(), requests[1]->GetRequestType());
  }
}

bool ShouldBubbleStartOpen(permissions::PermissionPrompt::Delegate* delegate) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipGestureSensitive)) {
    auto requests = delegate->Requests();
    const bool has_gesture =
        std::any_of(requests.begin(), requests.end(), [](auto* request) {
          return request->GetGestureType() ==
                 permissions::PermissionRequestGestureType::GESTURE;
        });
    if (has_gesture)
      return true;
  }
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipRequestTypeSensitive)) {
    // Notifications and geolocation are targeted here because they are usually
    // not necessary for the website to function correctly, so they can safely
    // be given less prominence.
    auto requests = delegate->Requests();
    const bool is_geolocation_or_notifications =
        std::any_of(requests.begin(), requests.end(), [](auto* request) {
          auto request_type = request->GetRequestType();
          return request_type == permissions::RequestType::kNotifications ||
                 request_type == permissions::RequestType::kGeolocation;
        });
    if (!is_geolocation_or_notifications)
      return true;
  }
  return false;
}

}  // namespace

PermissionRequestChip::PermissionRequestChip(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate)
    : PermissionChip(delegate,
                     GetPermissionIconId(delegate),
                     GetPermissionMessage(delegate),
                     ShouldBubbleStartOpen(delegate)),
      browser_(browser) {
  chip_shown_time_ = base::TimeTicks::Now();
  VerifyCameraAndMicRequest(delegate);
}

PermissionRequestChip::~PermissionRequestChip() {
  if (prompt_bubble_) {
    views::Widget* widget = prompt_bubble_->GetWidget();
    widget->RemoveObserver(this);
    widget->Close();
  }
}

void PermissionRequestChip::OpenBubble() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!prompt_bubble_);

  PermissionPromptBubbleView* prompt_bubble = new PermissionPromptBubbleView(
      browser_, delegate(), chip_shown_time_, PermissionPromptStyle::kChip);
  prompt_bubble->Show();
  prompt_bubble->GetWidget()->AddObserver(this);
  prompt_bubble_ = prompt_bubble;

  RecordChipButtonPressed();
}

views::BubbleDialogDelegateView*
PermissionRequestChip::GetPermissionPromptBubbleForTest() {
  return prompt_bubble_;
}

void PermissionRequestChip::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  PermissionChip::OnWidgetClosing(widget);
  prompt_bubble_ = nullptr;
}

bool PermissionRequestChip::IsBubbleShowing() const {
  return prompt_bubble_;
}

void PermissionRequestChip::RecordChipButtonPressed() {
    base::UmaHistogramLongTimes("Permissions.Chip.TimeToInteraction",
                                base::TimeTicks::Now() - chip_shown_time_);
}

BEGIN_METADATA(PermissionRequestChip, views::View)
END_METADATA
