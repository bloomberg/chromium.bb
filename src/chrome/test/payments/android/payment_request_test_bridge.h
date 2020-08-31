// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_ANDROID_PAYMENT_REQUEST_TEST_BRIDGE_H_
#define CHROME_TEST_PAYMENTS_ANDROID_PAYMENT_REQUEST_TEST_BRIDGE_H_

#include "base/callback.h"

namespace content {
class WebContents;
}

namespace payments {

// Sets a delegate on future Java PaymentRequests that returns the given values
// for queries about system state. If |use_delegate| is false, it disables the
// use of a testing delegate, returning to the production one.
void SetUseDelegateOnPaymentRequestForTesting(bool use_delegate,
                                              bool is_incognito,
                                              bool is_valid_ssl,
                                              bool is_web_contents_active,
                                              bool prefs_can_make_payment,
                                              bool skip_ui_for_basic_card);

// Get the WebContents of the Expandable Payment Handler for testing purpose, or
// null if nonexistent. To guarantee a non-null return, this function should be
// called only if: 1) PaymentRequest UI is opening. 2)
// ScrollToExpandPaymentHandler feature is enabled. 3) PaymentHandler is
// opening.
content::WebContents* GetPaymentHandlerWebContentsForTest();

bool ClickPaymentHandlerSecurityIconForTest();

// Confirms payment in minimal UI. Returns true on success.
bool ConfirmMinimalUIForTest();

// Dismisses the minimal UI. Returns true on success.
bool DismissMinimalUIForTest();

// Returns true when running on Android M or L.
bool IsAndroidMarshmallowOrLollipopForTest();

// Sets an observer on future Java PaymentRequests that will call these
// callbacks when the events occur.
void SetUseNativeObserverOnPaymentRequestForTesting(
    base::RepeatingClosure on_can_make_payment_called,
    base::RepeatingClosure on_can_make_payment_returned,
    base::RepeatingClosure on_has_enrolled_instrument_called,
    base::RepeatingClosure on_has_enrolled_instrument_returned,
    base::RepeatingClosure on_show_instruments_ready,
    base::RepeatingClosure on_not_supported_error,
    base::RepeatingClosure on_connection_terminated,
    base::RepeatingClosure on_abort_called,
    base::RepeatingClosure on_complete_called,
    base::RepeatingClosure on_minimal_ui_ready);

}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_ANDROID_PAYMENT_REQUEST_TEST_BRIDGE_H_
