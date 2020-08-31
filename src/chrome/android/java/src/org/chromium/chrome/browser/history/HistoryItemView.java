// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.favicon.IconType;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;

/**
 * The SelectableItemView for items displayed in the browsing history UI.
 */
public class HistoryItemView extends SelectableItemView<HistoryItem> implements LargeIconCallback {
    private ImageButton mRemoveButton;
    private VectorDrawableCompat mBlockedVisitDrawable;

    private HistoryManager mHistoryManager;
    private final RoundedIconGenerator mIconGenerator;
    private DefaultFaviconHelper mFaviconHelper;

    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private final int mEndPadding;

    private boolean mRemoveButtonVisible;
    private boolean mIsItemRemoved;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mMinIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(getResources());
        mEndPadding =
                context.getResources().getDimensionPixelSize(R.dimen.default_list_row_padding);

        mStartIconSelectedColorList =
                AppCompatResources.getColorStateList(context, R.color.default_icon_color_inverse);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mStartIconView.setImageResource(R.drawable.default_favicon);

        mRemoveButton = mEndButtonView;
        mRemoveButton.setImageResource(R.drawable.btn_delete_24dp);
        mRemoveButton.setContentDescription(getContext().getString((R.string.remove)));
        ApiCompatibilityUtils.setImageTintList(mRemoveButton,
                AppCompatResources.getColorStateList(
                        getContext(), R.color.default_icon_color_secondary));
        mRemoveButton.setOnClickListener(v -> remove());
        mRemoveButton.setScaleType(ScaleType.CENTER_INSIDE);
        mRemoveButton.setPaddingRelative(
                getResources().getDimensionPixelSize(
                        R.dimen.history_item_remove_button_lateral_padding),
                getPaddingTop(),
                getResources().getDimensionPixelSize(
                        R.dimen.history_item_remove_button_lateral_padding),
                getPaddingBottom());

        updateRemoveButtonVisibility();
    }

    @Override
    public void setItem(HistoryItem item) {
        if (getItem() == item) return;

        super.setItem(item);

        mTitleView.setText(item.getTitle());
        mDescriptionView.setText(item.getDomain());
        mIsItemRemoved = false;

        if (item.wasBlockedVisit()) {
            if (mBlockedVisitDrawable == null) {
                mBlockedVisitDrawable = VectorDrawableCompat.create(
                        getContext().getResources(), R.drawable.ic_block_red,
                        getContext().getTheme());
            }
            setStartIconDrawable(mBlockedVisitDrawable);
            mTitleView.setTextColor(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.default_red));
        } else {
            setStartIconDrawable(mFaviconHelper.getDefaultFaviconDrawable(
                    getContext().getResources(), item.getUrl(), true));
            if (mHistoryManager != null) requestIcon();

            mTitleView.setTextColor(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.default_text_color));
        }
    }

    /**
     * @param manager The HistoryManager associated with this item.
     */
    public void setHistoryManager(HistoryManager manager) {
        getItem().setHistoryManager(manager);
        if (mHistoryManager == manager) return;

        mHistoryManager = manager;
        if (!getItem().wasBlockedVisit()) requestIcon();
    }

    /**
     * @param helper The helper for fetching default favicons.
     */
    public void setFaviconHelper(DefaultFaviconHelper helper) {
        mFaviconHelper = helper;
    }

    /**
     * Removes the item associated with this view.
     */
    public void remove() {
        // If the remove button is double tapped, this method may be called twice.
        if (getItem() == null || mIsItemRemoved) return;

        mIsItemRemoved = true;
        getItem().remove();
    }

    /**
     * Should be called when the user's sign in state changes.
     */
    public void onSignInStateChange() {
        updateRemoveButtonVisibility();
    }

    /**
     * @param visible Whether the remove button should be visible. Note that this method will have
     *                no effect if the button is GONE because the signed in user is not allowed to
     *                delete browsing history.
     */
    public void setRemoveButtonVisible(boolean visible) {
        mRemoveButtonVisible = visible;
        if (!PrefServiceBridge.getInstance().getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
            return;
        }

        mRemoveButton.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void onClick() {
        if (getItem() != null) getItem().open();
    }

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
            @IconType int iconType) {
        Drawable drawable = FaviconUtils.getIconDrawableWithoutFilter(icon, getItem().getUrl(),
                fallbackColor, mIconGenerator, getResources(), mDisplayedIconSize);
        setStartIconDrawable(drawable);
    }

    private void requestIcon() {
        if (mHistoryManager == null || mHistoryManager.getLargeIconBridge() == null) return;

        mHistoryManager.getLargeIconBridge().getLargeIconForStringUrl(
                getItem().getUrl(), mMinIconSize, this);
    }

    private void updateRemoveButtonVisibility() {
        int removeButtonVisibility =
                !PrefServiceBridge.getInstance().getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)
                ? View.GONE
                : mRemoveButtonVisible ? View.VISIBLE : View.INVISIBLE;
        mRemoveButton.setVisibility(removeButtonVisibility);

        int endPadding = removeButtonVisibility == View.GONE ? mEndPadding : 0;
        ViewCompat.setPaddingRelative(mContentView, ViewCompat.getPaddingStart(mContentView),
                mContentView.getPaddingTop(), endPadding, mContentView.getPaddingBottom());
    }

    @VisibleForTesting
    View getRemoveButtonForTests() {
        return mRemoveButton;
    }
}
