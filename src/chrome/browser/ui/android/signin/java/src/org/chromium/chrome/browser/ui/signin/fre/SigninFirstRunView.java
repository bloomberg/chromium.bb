// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** View that wraps signin first run welcome screen and caches references to UI elements. **/
public class SigninFirstRunView extends LinearLayout {
    private ViewGroup mContent;
    private TextView mTitle;
    private TextView mSubtitle;
    private View mBrowserManagedHeader;
    private ProgressBar mProgressSpinner;
    private ViewGroup mSelectedAccount;
    private ImageView mExpandIcon;
    private ButtonCompat mContinueButton;
    private ButtonCompat mDismissButton;
    private TextViewWithClickableSpans mFooter;

    public SigninFirstRunView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mContent = findViewById(R.id.signin_fre_content);
        mTitle = findViewById(R.id.title);
        mSubtitle = findViewById(R.id.subtitle);
        mBrowserManagedHeader = findViewById(R.id.fre_browser_managed_by_organization);
        mProgressSpinner = findViewById(R.id.signin_fre_progress_spinner);
        mSelectedAccount = findViewById(R.id.signin_fre_selected_account);
        mExpandIcon = findViewById(R.id.signin_fre_selected_account_expand_icon);
        mContinueButton = findViewById(R.id.signin_fre_continue_button);
        mDismissButton = findViewById(R.id.signin_fre_dismiss_button);
        mFooter = findViewById(R.id.signin_fre_footer);

        if (FREMobileIdentityConsistencyFieldTrial.shouldHideTitleUntilPoliciesAreLoaded()) {
            mTitle.setVisibility(GONE);
        }
    }

    ViewGroup getContentView() {
        return mContent;
    }

    View getBrowserManagedHeaderView() {
        return mBrowserManagedHeader;
    }

    ProgressBar getProgressSpinnerView() {
        return mProgressSpinner;
    }

    ViewGroup getSelectedAccountView() {
        return mSelectedAccount;
    }

    ImageView getExpandIconView() {
        return mExpandIcon;
    }

    ButtonCompat getContinueButtonView() {
        return mContinueButton;
    }

    ButtonCompat getDismissButtonView() {
        return mDismissButton;
    }

    TextViewWithClickableSpans getFooterView() {
        return mFooter;
    }

    /** Updates the title and the subtitle for UI variations on native and policy load. **/
    void onNativeAndPoliciesLoaded() {
        Pair<Integer, Integer> titleAndSubtitleId =
                FREMobileIdentityConsistencyFieldTrial.getVariationTitleAndSubtitle();
        mTitle.setText(titleAndSubtitleId.first);
        mTitle.setVisibility(VISIBLE);
        if (titleAndSubtitleId.second != 0) {
            mSubtitle.setText(titleAndSubtitleId.second);
            mSubtitle.setVisibility(VISIBLE);
        }
    }
}
