// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller.h"

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the virtual card manual fallback
// bubble and the omnibox icon.
class VirtualCardManualFallbackBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public VirtualCardManualFallbackBubbleController,
      public content::WebContentsUserData<
          VirtualCardManualFallbackBubbleControllerImpl> {
 public:
  class ObserverForTest {
   public:
    virtual void OnBubbleShown() = 0;
  };

  ~VirtualCardManualFallbackBubbleControllerImpl() override;
  VirtualCardManualFallbackBubbleControllerImpl(
      const VirtualCardManualFallbackBubbleControllerImpl&) = delete;
  VirtualCardManualFallbackBubbleControllerImpl& operator=(
      const VirtualCardManualFallbackBubbleControllerImpl&) = delete;

  // Show the bubble view.
  void ShowBubble(const std::u16string& masked_card_identifier_string,
                  const CreditCard* virtual_card,
                  const std::u16string& virtual_card_cvc,
                  const gfx::Image& virtual_card_image);

  // Invoked when the omnibox icon is clicked.
  void ReshowBubble();

  // VirtualCardManualFallbackBubbleController:
  AutofillBubbleBase* GetBubble() const override;
  const gfx::Image& GetBubbleTitleIcon() const override;
  std::u16string GetBubbleTitleText() const override;
  std::u16string GetEducationalBodyLabel() const override;
  std::u16string GetVirtualCardNumberFieldLabel() const override;
  std::u16string GetExpirationDateFieldLabel() const override;
  std::u16string GetCardholderNameFieldLabel() const override;
  std::u16string GetCvcFieldLabel() const override;
  std::u16string GetValueForField(
      VirtualCardManualFallbackBubbleField field) const override;
  std::u16string GetFieldButtonTooltip(
      VirtualCardManualFallbackBubbleField field) const override;
  const CreditCard* GetVirtualCard() const override;
  bool ShouldIconBeVisible() const override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
  void OnFieldClicked(VirtualCardManualFallbackBubbleField field) override;

 protected:
  explicit VirtualCardManualFallbackBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      VirtualCardManualFallbackBubbleControllerImpl>;
  friend class VirtualCardManualFallbackBubbleViewsInteractiveUiTest;

  // Updates the system clipboard with the |text|.
  void UpdateClipboard(const std::u16string& text) const;

  // Logs which field was clicked when the user selects a field from the manual
  // fallback bubble.
  void LogVirtualCardManualFallbackBubbleFieldClicked(
      VirtualCardManualFallbackBubbleField field) const;

  // Returns whether the webcontents related to the controller is active.
  bool IsWebContentsActive();

  void SetEventObserverForTesting(ObserverForTest* observer_for_test);

  // The network + last four digits of card number for the related masked server
  // card.
  std::u16string masked_card_identifier_string_;

  // The cvc of the virtual card.
  std::u16string virtual_card_cvc_;

  // The virtual card to be displayed to the user in the bubble.
  CreditCard virtual_card_;

  // The virtual card image to be displayed as the title icon of the bubble.
  gfx::Image virtual_card_image_;

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // Whether the omnibox icon for the bubble should be visible.
  bool should_icon_be_visible_ = false;

  // Whether the bubble has been shown at least once. This needs to be reset
  // when there is a page navigation and bubble is therefore no longer
  // applicable.
  bool bubble_has_been_shown_ = false;

  // The field of the most-recently-clicked button, whose value
  // has been copied to the clipboard.
  absl::optional<VirtualCardManualFallbackBubbleField> clicked_field_;

  raw_ptr<ObserverForTest> observer_for_test_ = nullptr;

  base::WeakPtrFactory<VirtualCardManualFallbackBubbleControllerImpl>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_IMPL_H_
