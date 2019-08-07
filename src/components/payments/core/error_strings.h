// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ERROR_STRINGS_H_
#define COMPONENTS_PAYMENTS_CORE_ERROR_STRINGS_H_

namespace payments {
namespace errors {

// Please keep the list alphabetized.

// Only a single PaymentRequest UI can be displayed at a time.
extern const char kAnotherUiShowing[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Show().
extern const char kAttemptedInitializationTwice[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Abort().
extern const char kCannotAbortWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Abort().
extern const char kCannotAbortWithoutShow[];

// Mojo call PaymentRequest::Init() must precede
// PaymentRequest::CanMakePayment().
extern const char kCannotCallCanMakePaymentWithoutInit[];

// Mojo call PaymentRequest::Init() must precede
// PaymentRequest::HasEnrolledInstrument().
extern const char kCannotCallHasEnrolledInstrumentWithoutInit[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Complete().
extern const char kCannotCompleteWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Complete().
extern const char kCannotCompleteWithoutShow[];

// Mojo call PaymentRequest::Init() must precede PaymentRequest::Retry().
extern const char kCannotRetryWithoutInit[];

// Mojo call PaymentRequest::Show() must precede PaymentRequest::Retry().
extern const char kCannotRetryWithoutShow[];

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

// Chrome refuses to provide any payment information to a website with an
// invalid SSL certificate.
extern const char kInvalidSslCertificate[];

// Used when an invalid state is encountered generically.
extern const char kInvalidState[];

// Used when the {"supportedMethods": "", data: {}} is required, but not
// provided.
extern const char kMethodDataRequired[];

// Used when non-empty "supportedMethods": "" is required, but not provided.
extern const char kMethodNameRequired[];

// The PaymentRequest API is available only on secure origins.
extern const char kNotInASecureOrigin[];

// Used when PaymentRequest::Init() has not been called, but should have been.
extern const char kNotInitialized[];

// Used when PaymentRequest::Show() has not been called, but should have been.
extern const char kNotShown[];

// Chrome provides payment information only to a whitelist of origin types.
extern const char kProhibitedOrigin[];

// A long form explanation of Chrome's behavior in the case of kProhibitedOrigin
// or kInvalidSslCertificate error.
extern const char kProhibitedOriginOrInvalidSslExplanation[];

// Used when "total": {"label": "Total", "amount": {"currency": "USD", "value":
// "0.01"}} is required, bot not provided.
extern const char kTotalRequired[];

}  // namespace errors
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ERROR_STRINGS_H_
