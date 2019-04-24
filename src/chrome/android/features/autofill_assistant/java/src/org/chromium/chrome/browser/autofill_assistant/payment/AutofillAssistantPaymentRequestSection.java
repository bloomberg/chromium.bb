// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection;

class AutofillAssistantPaymentRequestSection extends PaymentRequestSection.OptionSection {
    /**
     * Constructs an AA Payment Request Section.
     *
     * @param context     Context to pull resources from.
     * @param sectionName Title of the section to display.
     * @param delegate    Delegate to alert when something changes in the dialog.
     */
    public AutofillAssistantPaymentRequestSection(
            Context context, String sectionName, SectionDelegate delegate) {
        super(context, sectionName, delegate);
        super.setTitleAppearance(R.style.TextAppearance_BlackTitle2);
        super.setSummaryAppearance(
                R.style.TextAppearance_BlackBody, R.style.TextAppearance_BlackBody);
    }

    @Override
    protected void updateControlLayout() {
        if (!mIsLayoutInitialized) return;

        if (mDisplayMode == DISPLAY_MODE_FOCUSED) {
            setPadding(/*left=*/mLargeSpacing, getPaddingTop(), /*right=*/mLargeSpacing,
                    getPaddingBottom());
        } else {
            setPadding(/*left=*/0, getPaddingTop(), /*right=*/0, getPaddingBottom());
        }

        super.updateControlLayout();
    }
}
