// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_upi_offer_bubble_views.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

SaveUPIOfferBubbleViews::SaveUPIOfferBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    autofill::SaveUPIBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller_);

  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_UPI_PROMPT_ACCEPT));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_UPI_PROMPT_REJECT));
}

SaveUPIOfferBubbleViews::~SaveUPIOfferBubbleViews() = default;

void SaveUPIOfferBubbleViews::Show() {
  ShowForReason(LocationBarBubbleDelegateView::DisplayReason::AUTOMATIC);
}

gfx::Size SaveUPIOfferBubbleViews::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool SaveUPIOfferBubbleViews::Accept() {
  controller_->OnAccept();
  return true;
}

base::string16 SaveUPIOfferBubbleViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_UPI_PROMPT_TITLE);
}

void SaveUPIOfferBubbleViews::Hide() {
  if (controller_)
    controller_->OnBubbleClosed();
  controller_ = nullptr;
  CloseBubble();
}

void SaveUPIOfferBubbleViews::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto label_view = std::make_unique<views::Label>(controller_->GetUpiId());
  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view->SetElideBehavior(gfx::ElideBehavior::ELIDE_EMAIL);
  AddChildView(std::move(label_view));
}
