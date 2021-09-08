// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_

#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

// Interface that exposes controller functionality to SaveAddressProfileView
// bubble.
class SaveUpdateAddressProfileBubbleController {
 public:
  virtual ~SaveUpdateAddressProfileBubbleController() = default;

  virtual std::u16string GetWindowTitle() const = 0;
  virtual const AutofillProfile& GetProfileToSave() const = 0;
  virtual const AutofillProfile* GetOriginalProfile() const = 0;
  virtual void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision) = 0;
  virtual void OnEditButtonClicked() = 0;
  virtual void OnBubbleClosed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
