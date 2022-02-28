// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VIEW_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VIEW_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentUIObserver;
class SecurePaymentConfirmationModel;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// src/tools/metrics/histograms/enums.xml.
enum class SecurePaymentConfirmationAuthenticationDialogResult {
  kCanceled = 0,
  kAccepted = 1,
  kClosed = 2,
  kMaxValue = kClosed,
};

// Draws the user interface in the secure payment confirmation flow. Owned by
// the SecurePaymentConfirmationController.
class SecurePaymentConfirmationView {
 public:
  using VerifyCallback = base::OnceCallback<void()>;
  using CancelCallback = base::OnceCallback<void()>;

  static base::WeakPtr<SecurePaymentConfirmationView> Create(
      const base::WeakPtr<PaymentUIObserver> payment_ui_observer);

  virtual ~SecurePaymentConfirmationView() = 0;

  virtual void ShowDialog(content::WebContents* web_contents,
                          base::WeakPtr<SecurePaymentConfirmationModel> model,
                          VerifyCallback verify_callback,
                          CancelCallback cancel_callback) = 0;
  virtual void OnModelUpdated() = 0;
  virtual void HideDialog() = 0;

 protected:
  SecurePaymentConfirmationView();

  base::WeakPtr<SecurePaymentConfirmationModel> model_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_VIEW_H_
