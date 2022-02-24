// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "content/public/browser/web_contents_user_data.h"

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_

namespace autofill {

class VirtualCardEnrollBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public VirtualCardEnrollBubbleController,
      public content::WebContentsUserData<
          VirtualCardEnrollBubbleControllerImpl> {
 public:
  VirtualCardEnrollBubbleControllerImpl(
      const VirtualCardEnrollBubbleControllerImpl&) = delete;
  VirtualCardEnrollBubbleControllerImpl& operator=(
      const VirtualCardEnrollBubbleControllerImpl&) = delete;
  ~VirtualCardEnrollBubbleControllerImpl() override;

  // Displays both the virtual card enroll bubble and its associated omnibox
  // icon. Sets virtual card enrollment fields as well as the closure for the
  // accept and decline bubble events.
  void ShowBubble(
      const VirtualCardEnrollmentFields* virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback);

  // Shows the bubble again if the users clicks the omnibox icon.
  void ReshowBubble();

  // VirtualCardEnrollBubbleController:
  std::u16string GetWindowTitle() const override;
  std::u16string GetExplanatoryMessage() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetDeclineButtonText() const override;
  std::u16string GetLearnMoreLinkText() const override;
  const VirtualCardEnrollmentFields* GetVirtualCardEnrollmentFields()
      const override;
  AutofillBubbleBase* GetVirtualCardEnrollBubbleView() const override;

  void OnAcceptButton() override;
  void OnDeclineButton() override;
  void OnLinkClicked(const GURL& url) override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
  bool IsIconVisible() const override;

 protected:
  explicit VirtualCardEnrollBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase::
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      VirtualCardEnrollBubbleControllerImpl>;

  // Contains more details regarding the sort of bubble to show the users.
  raw_ptr<const VirtualCardEnrollmentFields> virtual_card_enrollment_fields_;

  // True if the icon should be showing on the webpage
  bool should_show_icon_ = false;

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // Closure invoked when the user agrees to enroll in a virtual card.
  base::OnceClosure accept_virtual_card_callback_;

  // Closure invoked when the user rejects enrolling in a virtual card.
  base::OnceClosure decline_virtual_card_callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_H_
