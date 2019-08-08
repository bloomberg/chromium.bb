// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace test {

namespace ui {

const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepWorking[] = "working";
const char kEnrollmentStepSuccess[] = "success";
const char kEnrollmentStepLicenses[] = "license";
const char kEnrollmentStepDeviceAttributes[] = "attribute-prompt";
const char kEnrollmentStepADJoin[] = "ad-join";
const char kEnrollmentStepError[] = "error";
const char kEnrollmentStepDeviceAttributesError[] = "attribute-prompt-error";
const char kEnrollmentStepADJoinError[] = "active-directory-join-error";

}  // namespace ui

namespace values {

const char kLicenseTypePerpetual[] = "perpetual";
const char kLicenseTypeAnnual[] = "annual";
const char kLicenseTypeKiosk[] = "kiosk";

const char kAssetId[] = "asset_id";
const char kLocation[] = "location";

}  // namespace values

namespace {

const char* const kAllSteps[] = {ui::kEnrollmentStepSignin,
                                 ui::kEnrollmentStepWorking,
                                 ui::kEnrollmentStepLicenses,
                                 ui::kEnrollmentStepDeviceAttributes,
                                 ui::kEnrollmentStepSuccess,
                                 ui::kEnrollmentStepADJoin,
                                 ui::kEnrollmentStepError,
                                 ui::kEnrollmentStepADJoinError,
                                 ui::kEnrollmentStepDeviceAttributesError};

std::string StepVisibleExpression(const std::string& step) {
  return "document.getElementsByClassName('oauth-enroll-state-" + step +
         "').length > 0";
}

const std::initializer_list<base::StringPiece> kEnrollmentErrorRetryButtonPath =
    {"oauth-enroll-error-card", "submitButton"};

const std::initializer_list<base::StringPiece>
    kEnrollmentDeviceAttributesErrorButtonPath = {
        "oauth-enroll-attribute-prompt-error-card", "submitButton"};

}  // namespace

EnrollmentUIMixin::EnrollmentUIMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

EnrollmentUIMixin::~EnrollmentUIMixin() = default;

// Waits until specific enrollment step is displayed.
void EnrollmentUIMixin::WaitForStep(const std::string& step) {
  OobeJS().CreateWaiter(StepVisibleExpression(step))->Wait();
  for (const char* other : kAllSteps) {
    if (other != step) {
      ASSERT_FALSE(IsStepDisplayed(other));
    }
  }
}
// Returns true if there are any DOM elements with the given class.
bool EnrollmentUIMixin::IsStepDisplayed(const std::string& step) {
  return OobeJS().GetBool(StepVisibleExpression(step));
}

void EnrollmentUIMixin::SelectEnrollmentLicense(
    const std::string& license_type) {
  OobeJS().SelectRadioPath(
      {"oauth-enroll-license-ui", "license-option-" + license_type});
}

void EnrollmentUIMixin::UseSelectedLicense() {
  OobeJS().TapOnPath({"oauth-enroll-license-ui", "next"});
}

void EnrollmentUIMixin::ExpectErrorMessage(int error_message_id,
                                           bool can_retry) {
  const std::string element_path =
      GetOobeElementPath({"oauth-enroll-error-card"});
  const std::string message = OobeJS().GetString(element_path + ".textContent");
  ASSERT_TRUE(std::string::npos !=
              message.find(l10n_util::GetStringUTF8(error_message_id)));
  if (can_retry) {
    OobeJS().ExpectVisiblePath(kEnrollmentErrorRetryButtonPath);
  } else {
    OobeJS().ExpectHiddenPath(kEnrollmentErrorRetryButtonPath);
  }
}

void EnrollmentUIMixin::RetryAfterError() {
  OobeJS().TapOnPath(kEnrollmentErrorRetryButtonPath);
  WaitForStep(ui::kEnrollmentStepSignin);
}

void EnrollmentUIMixin::LeaveDeviceAttributeErrorScreen() {
  OobeJS().TapOnPath(kEnrollmentDeviceAttributesErrorButtonPath);
}

void EnrollmentUIMixin::SubmitDeviceAttributes(const std::string& asset_id,
                                               const std::string& location) {
  OobeJS().TypeIntoPath(asset_id, {"oauth-enroll-asset-id"});
  OobeJS().TypeIntoPath(location, {"oauth-enroll-location"});
  OobeJS().TapOn("enroll-attributes-submit-button");
}

}  // namespace test
}  // namespace chromeos
