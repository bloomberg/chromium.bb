// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_UPI_OFFER_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_UPI_OFFER_BUBBLE_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/autofill/payments/save_upi_bubble.h"
#include "chrome/browser/ui/autofill/payments/save_upi_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

// This class displays the "Remember your UPI ID?" bubble that is shown when the
// user submits a form with a UPI ID that Autofill
// has not previously saved. It includes the UPI ID that is being saved and a
// [Save] button. (Non-material UIs include a [No Thanks] button).
// UPI is a payments method
// https://en.wikipedia.org/wiki/Unified_Payments_Interface
class SaveUPIOfferBubbleViews : public autofill::SaveUPIBubble,
                                public LocationBarBubbleDelegateView {
 public:
  // The bubble will be anchored to |anchor_view|.
  SaveUPIOfferBubbleViews(views::View* anchor_view,
                          content::WebContents* web_contents,
                          autofill::SaveUPIBubbleController* controller);
  // Displays the bubble.
  void Show();

  // LocationBarBubbleDelegateView:
  gfx::Size CalculatePreferredSize() const override;

 private:
  // views::View:
  bool Accept() override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // autofill::SaveUPIBubble:
  void Hide() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  ~SaveUPIOfferBubbleViews() override;

  autofill::SaveUPIBubbleController* controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_UPI_OFFER_BUBBLE_VIEWS_H_
