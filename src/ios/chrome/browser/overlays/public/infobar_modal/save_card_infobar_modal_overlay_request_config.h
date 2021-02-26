// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_CARD_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_CARD_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

#include "components/autofill/core/browser/payments/legal_message_line.h"

class InfoBarIOS;
@class SaveCardMessageWithLinks;

namespace autofill {
class AutofillSaveCardInfoBarDelegateMobile;
}

namespace save_card_infobar_overlays {

// Configuration object for OverlayRequests for the modal UI for an infobar with
// a AutofillSaveCardInfoBarDelegateMobile.
class SaveCardModalRequestConfig
    : public OverlayRequestConfig<SaveCardModalRequestConfig> {
 public:
  ~SaveCardModalRequestConfig() override;

  // The card holder name of the card.
  base::string16 cardholder_name() const { return cardholder_name_; }

  // The expiration month of the card.
  base::string16 expiration_date_month() const {
    return expiration_date_month_;
  }

  // The expiration year of the card.
  base::string16 expiration_date_year() const { return expiration_date_year_; }

  // The last four digits of the card.
  base::string16 card_last_four_digits() const {
    return card_last_four_digits_;
  }

  // The resource ID for the icon that identifies the issuer of the card.
  int issuer_icon_id() const { return issuer_icon_id_; }

  // The legal disclaimer shown at the bottom of the modal.
  NSArray<SaveCardMessageWithLinks*>* legal_message_lines() const {
    return legal_message_lines_;
  }

  // Whether the current card is already saved.
  bool current_card_saved() const { return current_card_saved_; }

  // Whether the action is an upload or a local save.
  bool should_upload_credentials() const { return should_upload_credentials_; }

 private:
  OVERLAY_USER_DATA_SETUP(SaveCardModalRequestConfig);
  explicit SaveCardModalRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // Return an array of UI SaveCardMessageWithLinks model objects for
  // |delegate|'s legal_message_lines_.
  NSMutableArray<SaveCardMessageWithLinks*>* LegalMessagesForModal(
      autofill::AutofillSaveCardInfoBarDelegateMobile* delegate);

  // The InfoBar causing this modal.
  InfoBarIOS* infobar_ = nullptr;
  // Configuration data extracted from |infobar_|'s save card delegate.
  base::string16 cardholder_name_;
  base::string16 expiration_date_month_;
  base::string16 expiration_date_year_;
  base::string16 card_last_four_digits_;
  int issuer_icon_id_;
  NSArray<SaveCardMessageWithLinks*>* legal_message_lines_;
  bool current_card_saved_ = false;
  bool should_upload_credentials_ = false;
};

}  // namespace save_card_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_CARD_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
