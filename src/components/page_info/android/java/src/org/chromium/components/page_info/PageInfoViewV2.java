// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import java.util.Arrays;
import java.util.List;

/**
 * Represents the view inside the second version of the page info popup.
 */
public class PageInfoViewV2 extends PageInfoView {
    // Components specific to this PageInfoView
    private LinearLayout mRowWrapper;
    private PageInfoRowView mConnectionRow;
    private PageInfoRowView mPerformanceRow;
    private PageInfoRowView mPermissionsRow;
    private PageInfoRowView mCookiesRow;

    public PageInfoViewV2(Context context, PageInfoView.PageInfoViewParams params) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info_v2, this, true);
        init(params);
    }

    @Override
    protected void init(PageInfoView.PageInfoViewParams params) {
        super.init(params);
        mRowWrapper = findViewById(R.id.page_info_row_wrapper);
        initializePageInfoViewChild(mRowWrapper, true, 0f, null);
    }

    @Override
    protected void initUrlTitle(PageInfoView.PageInfoViewParams params) {
        super.initUrlTitle(params);
        // Adjust the mUrlTitle
        mUrlTitle.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
    }

    @Override
    protected void initConnection(PageInfoView.PageInfoViewParams params) {
        mConnectionRow = findViewById(R.id.page_info_connection_row);
    }

    @Override
    protected void initPerformance(PageInfoView.PageInfoViewParams params) {
        mPerformanceRow = findViewById(R.id.page_info_performance_row);
    }

    @Override
    protected void initPermissions(PageInfoView.PageInfoViewParams params) {
        mPermissionsRow = findViewById(R.id.page_info_permissions_row);
    }

    @Override
    protected void initCookies(PageInfoView.PageInfoViewParams params) {
        mCookiesRow = findViewById(R.id.page_info_cookies_row);
        mOnUiClosingCallback = params.onUiClosingCallback;
    }

    public PageInfoRowView getConnectionRowView() {
        return mConnectionRow;
    }

    public PageInfoRowView getPerformanceRowView() {
        return mPerformanceRow;
    }

    public PageInfoRowView getPermissionsRowView() {
        return mPermissionsRow;
    }

    public PageInfoRowView getCookiesRowView() {
        return mCookiesRow;
    }

    /**
     * Create a list of all the views which we want to individually fade in.
     */
    @Override
    protected List<View> collectAnimatableViews() {
        return Arrays.asList(mUrlTitle, mPreviewMessage, mPreviewStaleTimestamp,
                mPreviewLoadOriginal, mPreviewSeparator, mInstantAppButton, mRowWrapper,
                mSiteSettingsButton);
    }
}
