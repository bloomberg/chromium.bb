// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ERROR_STRINGS_H_
#define COMPONENTS_PAYMENTS_CORE_ERROR_STRINGS_H_

namespace payments {
namespace errors {

// These strings are referenced from both C++ and Java (through the
// auto-generated file ErrorStrings.java).

// Please keep the list alphabetized.

// Only a single PaymentRequest UI can be displayed at a time.
extern const char kAnotherUiShowing[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Show().
extern const char kAttemptedInitializationTwice[];

// Payment Request UI must be shown in the foreground tab, as a result of user
// interaction.
extern const char kCannotShowInBackgroundTab[];

// Mojo call PaymentRequest::Show() cannot happen more than once per Mojo pipe.
extern const char kCannotShowTwice[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Show().
extern const char kCannotShowWithoutInit[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::UpdateWith().
extern const char kCannotUpdateWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::UpdateWith().
extern const char kCannotUpdateWithoutShow[];

// Used when an invalid state is encountered generically.
extern const char kInvalidState[];

// The payment handler responded with an empty "details" field.
extern const char kMissingDetailsFromPaymentApp[];

// The payment handler responded with an empty "methodName" field.
extern const char kMissingMethodNameFromPaymentApp[];

// The PaymentRequest API is available only on secure origins.
extern const char kNotInASecureOrigin[];

// The payment handler responded with an empty "payer name" field.
extern const char kPayerNameEmpty[];

// The payment handler responded with an empty "payer email" field.
extern const char kPayerEmailEmpty[];

// The payment handler responded with an empty "payer phone" field.
extern const char kPayerPhoneEmpty[];

// Chrome provides payment information only to a whitelist of origin types.
extern const char kProhibitedOrigin[];

// A long form explanation of Chrome"s behavior in the case of kProhibitedOrigin
// or kInvalidSslCertificate error.
extern const char kProhibitedOriginOrInvalidSslExplanation[];

// The payment handler responded with an invalid shipping address.
extern const char kShippingAddressInvalid[];

// The payment handler responded with an empty "shipping option" field.
extern const char kShippingOptionEmpty[];

// Used when rejecting show() with NotSupportedError, because the user did not
// have all valid autofill data.
extern const char kStrictBasicCardShowReject[];

// Used when "total": {"label": "Total", "amount": {"currency": "USD", "value":
// "0.01"}} is required, bot not provided.
extern const char kTotalRequired[];

// Used when user dismissed the Payment Request dialog.
extern const char kUserCancelled[];

}  // namespace errors
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ERROR_STRINGS_H_
