// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_utils.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"

namespace autofill {

namespace {

// Used in the IDS_ space as a placeholder for resources that don't exist.
constexpr int kResourceNotFoundId = 0;

// Used to fetch the |icon_id| corresponding the the |name| resource.
const struct {
  const char* name;
  int icon_id;
} kDataResources[] = {
    {kAmericanExpressCard, IDR_AUTOFILL_CC_AMEX},
    {kDinersCard, IDR_AUTOFILL_CC_DINERS},
    {kDiscoverCard, IDR_AUTOFILL_CC_DISCOVER},
    {kEloCard, IDR_AUTOFILL_CC_ELO},
    {kGenericCard, IDR_AUTOFILL_CC_GENERIC},
    {kJCBCard, IDR_AUTOFILL_CC_JCB},
    {kMasterCard, IDR_AUTOFILL_CC_MASTERCARD},
    {kMirCard, IDR_AUTOFILL_CC_MIR},
    {kUnionPay, IDR_AUTOFILL_CC_UNIONPAY},
    {kVisaCard, IDR_AUTOFILL_CC_VISA},
    {kGoogleIssuedCard, IDR_AUTOFILL_GOOGLE_ISSUED_CARD},
#if defined(OS_ANDROID)
    {"httpWarning", IDR_ANDROID_AUTOFILL_HTTP_WARNING},
    {"httpsInvalid", IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING},
    {"scanCreditCardIcon", IDR_ANDROID_AUTOFILL_CC_SCAN_NEW},
    {"settings", IDR_ANDROID_AUTOFILL_SETTINGS},
    {"create", IDR_ANDROID_AUTOFILL_CREATE},
#endif  // OS_ANDROID
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"googlePay", IDR_AUTOFILL_GOOGLE_PAY},
#if !defined(OS_ANDROID)
    {"googlePayDark", IDR_AUTOFILL_GOOGLE_PAY_DARK},
#endif  // NOT OS_ANDROID
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

}  // namespace

int GetIconResourceID(const std::string& resource_name) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (resource_name == "googlePay" || resource_name == "googlePayDark") {
    return 0;
  }
#endif
  int result = kResourceNotFoundId;
  for (const auto& kDataResource : kDataResources) {
    if (resource_name == kDataResource.name) {
      result = kDataResource.icon_id;
      break;
    }
  }

  return result;
}

}  // namespace autofill
