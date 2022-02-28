// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_MODEL_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_MODEL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace payments {

// The data model for the secure payment confirmation flow. Owned by the
// SecurePaymentConfirmationController.
class SecurePaymentConfirmationModel {
 public:
  SecurePaymentConfirmationModel();
  ~SecurePaymentConfirmationModel();

  // Disallow copy and assign.
  SecurePaymentConfirmationModel(const SecurePaymentConfirmationModel& other) =
      delete;
  SecurePaymentConfirmationModel& operator=(
      const SecurePaymentConfirmationModel& other) = delete;

  // Title, e.g. "Use TouchID to verify and complete your purchase?"
  const std::u16string& title() const { return title_; }
  void set_title(const std::u16string& title) { title_ = title; }

  // Label for the merchant row, e.g. "Store".
  const std::u16string& merchant_label() const { return merchant_label_; }
  void set_merchant_label(const std::u16string& merchant_label) {
    merchant_label_ = merchant_label;
  }

  // Label for the merchant row value, e.g. "merchant.com"
  const std::u16string& merchant_value() const { return merchant_value_; }
  void set_merchant_value(const std::u16string& merchant_value) {
    merchant_value_ = merchant_value;
  }

  // Label for the instrument row, e.g. "Payment".
  const std::u16string& instrument_label() const { return instrument_label_; }
  void set_instrument_label(const std::u16string& instrument_label) {
    instrument_label_ = instrument_label;
  }

  // Label for the instrument row value, e.g. "Mastercard ****4444"
  const std::u16string& instrument_value() const { return instrument_value_; }
  void set_instrument_value(const std::u16string& instrument_value) {
    instrument_value_ = instrument_value;
  }

  // Instrument icon.
  const SkBitmap* instrument_icon() const { return instrument_icon_; }
  void set_instrument_icon(const SkBitmap* instrument_icon) {
    instrument_icon_ = instrument_icon;
  }

  // Label for the total row, e.g. "Total".
  const std::u16string& total_label() const { return total_label_; }
  void set_total_label(const std::u16string& total_label) {
    total_label_ = total_label;
  }

  // Label for the total row value, e.g. "$20.00 USD"
  const std::u16string& total_value() const { return total_value_; }
  void set_total_value(const std::u16string& total_value) {
    total_value_ = total_value;
  }

  // Label for the verify button, e.g. "Verify".
  const std::u16string& verify_button_label() const {
    return verify_button_label_;
  }
  void set_verify_button_label(const std::u16string& verify_button_label) {
    verify_button_label_ = verify_button_label;
  }

  // Label for the cancel button, e.g. "Cancel".
  const std::u16string& cancel_button_label() const {
    return cancel_button_label_;
  }
  void set_cancel_button_label(const std::u16string& cancel_button_label) {
    cancel_button_label_ = cancel_button_label;
  }

  // Progress bar visibility.
  bool progress_bar_visible() const { return progress_bar_visible_; }
  void set_progress_bar_visible(bool progress_bar_visible) {
    progress_bar_visible_ = progress_bar_visible;
  }

  // Verify button enabled state.
  bool verify_button_enabled() const { return verify_button_enabled_; }
  void set_verify_button_enabled(bool verify_button_enabled) {
    verify_button_enabled_ = verify_button_enabled;
  }

  // Verify button visibility.
  bool verify_button_visible() const { return verify_button_visible_; }
  void set_verify_button_visible(bool verify_button_visible) {
    verify_button_visible_ = verify_button_visible;
  }

  // Cancel button enabled state.
  bool cancel_button_enabled() const { return cancel_button_enabled_; }
  void set_cancel_button_enabled(bool cancel_button_enabled) {
    cancel_button_enabled_ = cancel_button_enabled;
  }

  // Cancel button visibility.
  bool cancel_button_visible() const { return cancel_button_visible_; }
  void set_cancel_button_visible(bool cancel_button_visible) {
    cancel_button_visible_ = cancel_button_visible;
  }

  base::WeakPtr<SecurePaymentConfirmationModel> GetWeakPtr();

 private:
  std::u16string title_;

  std::u16string merchant_label_;
  std::u16string merchant_value_;

  std::u16string instrument_label_;
  std::u16string instrument_value_;
  raw_ptr<const SkBitmap> instrument_icon_ = nullptr;

  std::u16string total_label_;
  std::u16string total_value_;

  std::u16string verify_button_label_;
  std::u16string cancel_button_label_;

  bool progress_bar_visible_ = false;

  bool verify_button_enabled_ = true;
  bool verify_button_visible_ = true;

  bool cancel_button_enabled_ = true;
  bool cancel_button_visible_ = true;

  base::WeakPtrFactory<SecurePaymentConfirmationModel> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_MODEL_H_
