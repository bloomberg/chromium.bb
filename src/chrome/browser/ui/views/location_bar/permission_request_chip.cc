// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
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
    return requests[0]->GetIconForChip();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->request_type())
             ? requests[0]->GetIconForChip()
             : requests[1]->GetIconForChip();
}

const gfx::VectorIcon& GetBlockedPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  auto requests = delegate->Requests();
  if (requests.size() == 1)
    return requests[0]->GetBlockedIconForChip();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->request_type())
             ? requests[0]->GetBlockedIconForChip()
             : requests[1]->GetBlockedIconForChip();
}

std::u16string GetPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  auto requests = delegate->Requests();

  return requests.size() == 1
             ? requests[0]->GetRequestChipText().value()
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
    DCHECK(IsCameraOrMicPermission(requests[0]->request_type()));
    DCHECK(IsCameraOrMicPermission(requests[1]->request_type()));
    DCHECK_NE(requests[0]->request_type(), requests[1]->request_type());
  }
}

}  // namespace

PermissionRequestChip::PermissionRequestChip(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate,
    bool should_bubble_start_open)
    : PermissionChip(
          delegate,
          {GetPermissionIconId(delegate), GetBlockedPermissionIconId(delegate),
           GetPermissionMessage(delegate), should_bubble_start_open,
           base::FeatureList::IsEnabled(
               permissions::features::kPermissionChipIsProminentStyle),
           OmniboxChipButton::Theme::kNormalVisibility,
           /*should_expand=*/true}),
      browser_(browser) {
  chip_shown_time_ = base::TimeTicks::Now();
  VerifyCameraAndMicRequest(delegate);
}

PermissionRequestChip::~PermissionRequestChip() = default;

views::View* PermissionRequestChip::CreateBubble() {
  PermissionPromptBubbleView* prompt_bubble = new PermissionPromptBubbleView(
      browser_, delegate(), chip_shown_time_, PermissionPromptStyle::kChip);
  prompt_bubble->SetOnBubbleDismissedByUserCallback(base::BindOnce(
      &PermissionRequestChip::OnPromptBubbleDismissed, base::Unretained(this)));
  prompt_bubble->Show();
  prompt_bubble->GetWidget()->AddObserver(this);

  RecordChipButtonPressed();

  return prompt_bubble;
}

void PermissionRequestChip::RecordChipButtonPressed() {
  base::UmaHistogramMediumTimes("Permissions.Chip.TimeToInteraction",
                                base::TimeTicks::Now() - chip_shown_time_);
}

BEGIN_METADATA(PermissionRequestChip, views::View)
END_METADATA
