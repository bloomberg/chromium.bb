// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_mobile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

VirtualCardEnrollBubbleControllerImpl::VirtualCardEnrollBubbleControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<VirtualCardEnrollBubbleControllerImpl>(
          *web_contents) {}

VirtualCardEnrollBubbleControllerImpl::
    ~VirtualCardEnrollBubbleControllerImpl() = default;

void VirtualCardEnrollBubbleControllerImpl::ShowBubble(
    const VirtualCardEnrollmentFields* virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  virtual_card_enrollment_fields_ = virtual_card_enrollment_fields;
  accept_virtual_card_callback_ = std::move(accept_virtual_card_callback);
  decline_virtual_card_callback_ = std::move(decline_virtual_card_callback);
  is_user_gesture_ = false;
  Show();
}

void VirtualCardEnrollBubbleControllerImpl::ReshowBubble() {
  DCHECK(IsIconVisible());

  if (bubble_view())
    return;

  is_user_gesture_ = true;
  Show();
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL);
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetExplanatoryMessage()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_CONTENT_LABEL,
      GetLearnMoreLinkText());
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetAcceptButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_ACCEPT_BUTTON_LABEL);
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetDeclineButtonText()
    const {
  return l10n_util::GetStringUTF16(
      virtual_card_enrollment_fields_->virtual_card_enrollment_source ==
              VirtualCardEnrollmentSource::kSettingsPage
          ? IDS_CANCEL
          : IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DECLINE_BUTTON_LABEL);
}

std::u16string VirtualCardEnrollBubbleControllerImpl::GetLearnMoreLinkText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL);
}

const VirtualCardEnrollmentFields*
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardEnrollmentFields() const {
  return virtual_card_enrollment_fields_.get();
}

AutofillBubbleBase*
VirtualCardEnrollBubbleControllerImpl::GetVirtualCardEnrollBubbleView() const {
  return bubble_view();
}

void VirtualCardEnrollBubbleControllerImpl::OnAcceptButton() {
  std::move(accept_virtual_card_callback_).Run();
  decline_virtual_card_callback_.Reset();

  should_show_icon_ = false;
}

void VirtualCardEnrollBubbleControllerImpl::OnDeclineButton() {
  std::move(decline_virtual_card_callback_).Run();
  accept_virtual_card_callback_.Reset();

  should_show_icon_ = false;
}

void VirtualCardEnrollBubbleControllerImpl::OnLinkClicked(const GURL& url) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void VirtualCardEnrollBubbleControllerImpl::OnBubbleClosed(
    PaymentsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
}

bool VirtualCardEnrollBubbleControllerImpl::IsIconVisible() const {
  return should_show_icon_;
}

PageActionIconType
VirtualCardEnrollBubbleControllerImpl::GetPageActionIconType() {
  return PageActionIconType::kVirtualCardEnroll;
}

void VirtualCardEnrollBubbleControllerImpl::DoShowBubble() {
#if BUILDFLAG(IS_ANDROID)
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  DCHECK(infobar_manager);
  infobar_manager->RemoveAllInfoBars(true);
  infobar_manager->AddInfoBar(CreateVirtualCardEnrollmentInfoBarMobile(
      std::make_unique<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>(
          this)));
#else
  // If bubble is already showing for another card, close it.
  if (bubble_view())
    HideBubble();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowVirtualCardEnrollBubble(web_contents(), this,
                                                    is_user_gesture_));
  DCHECK(bubble_view());
  should_show_icon_ = true;
#endif  // BUILDFLAG(IS_ANDROID)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VirtualCardEnrollBubbleControllerImpl);

}  // namespace autofill
