// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

class InfoBarIOS;

namespace autofill {
class AutofillSaveAddressProfilePromptDelegateMobile;
}

namespace save_address_profile_infobar_overlays {

// Configuration object for OverlayRequests for the modal UI for an infobar with
// a AutofillSaveAddressProfilePromptDelegateMobile.
class SaveAddressProfileModalRequestConfig
    : public OverlayRequestConfig<SaveAddressProfileModalRequestConfig> {
 public:
  ~SaveAddressProfileModalRequestConfig() override;

  // Returns full name stored in the |profile_|.
  std::u16string GetProfileName() const;

  // Returns address line 1 stored in the |profile_|.
  std::u16string GetProfileAddressLine1() const;

  // Returns address line 2 stored in the |profile_|.
  std::u16string GetProfileAddressLine2() const;

  // Returns city stored in the |profile_|.
  std::u16string GetProfileCity() const;

  // Returns state stored in the |profile_|.
  std::u16string GetProfileState() const;

  // Returns country stored in the |profile_|.
  std::u16string GetProfileCountry() const;

  // Returns zip code stored in the |profile_|.
  std::u16string GetProfileZip() const;

  // Returns phone number stored in the |profile_|.
  std::u16string GetProfilePhone() const;

  // Returns email stored in the |profile_|.
  std::u16string GetProfileEmail() const;

  // Whether the current address profile is already saved.
  bool current_address_profile_saved() const {
    return current_address_profile_saved_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(SaveAddressProfileModalRequestConfig);
  explicit SaveAddressProfileModalRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this modal.
  InfoBarIOS* infobar_ = nullptr;

  // Address Profile pointer.
  const autofill::AutofillProfile* profile_ = nullptr;

  // True if the address profile is saved.
  bool current_address_profile_saved_ = false;
};

}  // namespace save_address_profile_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
