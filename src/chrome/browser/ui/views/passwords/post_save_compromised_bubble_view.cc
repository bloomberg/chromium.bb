// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

PostSaveCompromisedBubbleView::PostSaveCompromisedBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  base::string16 button = controller_.GetButtonText();
  if (button.empty()) {
    SetButtons(ui::DIALOG_BUTTON_NONE);
  } else {
    SetButtons(ui::DIALOG_BUTTON_OK);
    SetButtonLabel(ui::DIALOG_BUTTON_OK, std::move(button));
  }

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(controller_.GetBody());
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  gfx::Range range = controller_.GetSettingLinkRange();
  if (!range.is_empty()) {
    label->AddStyleRange(
        range,
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &PostSaveCompromisedBubbleController::OnSettingsClicked,
            base::Unretained(&controller_))));
  }
  AddChildView(std::move(label));

  SetAcceptCallback(
      base::BindOnce(&PostSaveCompromisedBubbleController::OnAccepted,
                     base::Unretained(&controller_)));
}

PostSaveCompromisedBubbleView::~PostSaveCompromisedBubbleView() = default;

PostSaveCompromisedBubbleController*
PostSaveCompromisedBubbleView::GetController() {
  return &controller_;
}

const PostSaveCompromisedBubbleController*
PostSaveCompromisedBubbleView::GetController() const {
  return &controller_;
}

void PostSaveCompromisedBubbleView::AddedToWidget() {
  // Set the header image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(controller_.GetImageID(/*dark=*/false)),
      *bundle.GetImageSkiaNamed(controller_.GetImageID(/*dark=*/true)),
      base::BindRepeating(
          [](PostSaveCompromisedBubbleView* view) {
            return view->GetBubbleFrameView()->GetBackgroundColor();
          },
          this));

  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}
