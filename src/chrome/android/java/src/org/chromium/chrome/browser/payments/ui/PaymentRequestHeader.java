// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.omnibox.OmniboxUrlEmphasizer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.TintedDrawable;

/** This class represents a bar to display at the top of the payment request UI. */
public class PaymentRequestHeader extends FrameLayout {
    private Context mContext;

    /** Constructor for when the PaymentRequestHeader is inflated from XML. */
    public PaymentRequestHeader(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    /**
     * Sets the bitmap of the icon on the left of the header.
     *
     * @param bitmap The bitmap to display.
     */
    public void setTitleBitmap(Bitmap bitmap) {
        ((ImageView) findViewById(R.id.icon_view)).setImageBitmap(bitmap);
    }

    /**
     * Sets the title and origin on the header.
     *
     * @param title         The title to display on the header.
     * @param origin        The origin to display on the header.
     * @param securityLevel The security level of the page that invoked PaymentRequest.
     */
    public void setTitleAndOrigin(String title, String origin, int securityLevel) {
        ((TextView) findViewById(R.id.page_title)).setText(title);

        TextView hostName = (TextView) findViewById(R.id.hostname);
        Spannable url = new SpannableStringBuilder(origin);
        OmniboxUrlEmphasizer.emphasizeUrl(url, mContext.getResources(),
                Profile.getLastUsedProfile(), securityLevel, false /* isInternalPage */,
                true /* useDarkColors */, true /* emphasizeHttpsScheme */);
        hostName.setText(url);

        if (origin.startsWith(UrlConstants.HTTPS_URL_PREFIX)) {
            // Add a lock icon.
            ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(hostName,
                    TintedDrawable.constructTintedDrawable(
                            mContext, R.drawable.omnibox_https_valid, R.color.google_green_700),
                    null, null, null);

            // Remove left padding to align left compound drawable with the title. Note that the
            // left compound drawable has transparent boundary.
            hostName.setPaddingRelative(0, 0, 0, 0);
        }
    }
}