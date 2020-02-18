// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.res.TypedArray;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceViewHolder;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * A preference that navigates to an URL.
 */
public class HyperlinkPreference extends Preference {

    private final int mUrlResId;
    private final int mColor;
    private final boolean mImitateWebLink;

    public HyperlinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs,
                R.styleable.HyperlinkPreference, 0, 0);
        mUrlResId = a.getResourceId(R.styleable.HyperlinkPreference_url, 0);
        mImitateWebLink = a.getBoolean(R.styleable.HyperlinkPreference_imitateWebLink, false);
        a.recycle();
        mColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.default_text_color_link);
    }

    @Override
    protected void onClick() {
        CustomTabActivity.showInfoPage(WindowAndroid.activityFromContext(getContext()),
                LocalizationUtils.substituteLocalePlaceholder(getContext().getString(mUrlResId)));
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setSingleLine(false);

        if (mImitateWebLink) {
            setSelectable(false);

            titleView.setClickable(true);
            titleView.setTextColor(mColor);
            titleView.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    HyperlinkPreference.this.onClick();
                }
            });
        }
    }
}
