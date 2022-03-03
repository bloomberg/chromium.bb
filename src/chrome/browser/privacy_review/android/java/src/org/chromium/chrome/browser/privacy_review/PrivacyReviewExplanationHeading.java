// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_review;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

/** A custom view for a heading of a setting explanation for the privacy review. */
public class PrivacyReviewExplanationHeading extends LinearLayout {
    public PrivacyReviewExplanationHeading(Context context, AttributeSet attrs) {
        super(context, attrs);

        View view = LayoutInflater.from(context).inflate(
                R.layout.privacy_review_explanation_heading, this);

        TypedArray styledAttrs = context.obtainStyledAttributes(
                attrs, R.styleable.PrivacyReviewExplanationHeading, 0, 0);

        TextView title = (TextView) view.findViewById(R.id.title);
        title.setText(styledAttrs.getText(R.styleable.PrivacyReviewExplanationHeading_titleText));

        styledAttrs.recycle();
    }
}
