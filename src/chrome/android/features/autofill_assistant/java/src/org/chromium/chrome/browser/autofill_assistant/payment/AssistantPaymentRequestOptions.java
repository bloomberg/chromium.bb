// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.support.annotation.Nullable;

/**
 * Options for the Assistant Payment Request.
 */
class AssistantPaymentRequestOptions {
    final boolean mRequestPayerName;
    final boolean mRequestPayerEmail;
    final boolean mRequestPayerPhone;
    final boolean mRequestShipping;
    final String[] mSupportedBasicCardNetworks;
    @Nullable
    final String mDefaultEmail;

    AssistantPaymentRequestOptions(boolean requestPayerName, boolean requestPayerEmail,
            boolean requestPayerPhone, boolean requestShipping, String[] supportedBasicCardNetworks,
            @Nullable String defaultEmail) {
        this.mRequestPayerName = requestPayerName;
        this.mRequestPayerEmail = requestPayerEmail;
        this.mRequestPayerPhone = requestPayerPhone;
        this.mRequestShipping = requestShipping;
        this.mSupportedBasicCardNetworks = supportedBasicCardNetworks;
        this.mDefaultEmail = defaultEmail;
    }
}
