// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.AppCompatImageButton;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.TintedDrawable;

/**
 * Default implementation of SelectableItemViewBase. Contains a start icon, title, description, and
 * optional end icon (GONE by default). Views may be accessed through protected member variables.
 *
 * @param <E> The type of the item associated with this SelectableItemViewBase.
 */
public abstract class SelectableItemView<E> extends SelectableItemViewBase<E> {
    protected final int mDefaultLevel;
    protected final int mSelectedLevel;
    protected final AnimatedVectorDrawableCompat mCheckDrawable;

    /**
     * The LinearLayout containing the rest of the views for the selectable item.
     */
    protected LinearLayout mContentView;

    /**
     * An icon displayed at the start of the item row.
     */
    protected ImageView mStartIconView;

    /**
     * An optional button displayed at the end of the item row, GONE by default.
     */
    protected AppCompatImageButton mEndButtonView;

    /**
     * A title line displayed between the start and (optional) end icon.
     */
    protected TextView mTitleView;

    /**
     * A description line displayed below the title line.
     */
    protected TextView mDescriptionView;

    /**
     * The color state list for the start icon view when the item is selected.
     */
    protected ColorStateList mStartIconSelectedColorList;

    private Drawable mStartIconDrawable;

    /**
     * Constructor for inflating from XML.
     */
    public SelectableItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mStartIconSelectedColorList = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_inverse);
        mDefaultLevel = getResources().getInteger(R.integer.list_item_level_default);
        mSelectedLevel = getResources().getInteger(R.integer.list_item_level_selected);
        mCheckDrawable = AnimatedVectorDrawableCompat.create(
                getContext(), R.drawable.ic_check_googblue_24dp_animated);
    }

    // FrameLayout implementations.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        LayoutInflater.from(getContext()).inflate(R.layout.modern_list_item_view, this);

        mContentView = findViewById(R.id.content);
        mStartIconView = findViewById(R.id.start_icon);
        mEndButtonView = findViewById(R.id.end_button);
        mTitleView = findViewById(R.id.title);
        mDescriptionView = findViewById(R.id.description);

        if (mStartIconView != null) {
            mStartIconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            ApiCompatibilityUtils.setImageTintList(mStartIconView, getDefaultStartIconTint());
        }
    }

    /**
     * Set drawable for the start icon view. Note that you may need to use this method instead of
     * mIconView#setImageDrawable to ensure icon view is correctly set in selection mode.
     */
    protected void setStartIconDrawable(Drawable iconDrawable) {
        mStartIconDrawable = iconDrawable;
        updateView(false);
    }

    /**
     * Returns the drawable set for the start icon view, if any.
     */
    protected Drawable getStartIconDrawable() {
        return mStartIconDrawable;
    }

    /**
     * Update start icon image and background based on whether this item is selected.
     */
    @Override
    protected void updateView(boolean animate) {
        // TODO(huayinz): Refactor this method so that mIconView is not exposed to subclass.
        if (mStartIconView == null) return;

        if (isChecked()) {
            mStartIconView.getBackground().setLevel(mSelectedLevel);
            mStartIconView.setImageDrawable(mCheckDrawable);
            ApiCompatibilityUtils.setImageTintList(mStartIconView, mStartIconSelectedColorList);
            if (animate) mCheckDrawable.start();
        } else {
            mStartIconView.getBackground().setLevel(mDefaultLevel);
            mStartIconView.setImageDrawable(mStartIconDrawable);
            ApiCompatibilityUtils.setImageTintList(mStartIconView, getDefaultStartIconTint());
        }
    }

    /**
     * @return The {@link ColorStateList} used to tint the start icon drawable set via
     *         {@link #setStartIconDrawable(Drawable)} when the item is not selected.
     */
    protected @Nullable ColorStateList getDefaultStartIconTint() {
        return null;
    }

    @VisibleForTesting
    public void endAnimationsForTests() {
        mCheckDrawable.stop();
    }

    /**
     * Sets the icon for the image view: the default icon if unselected, the check mark if selected.
     *
     * @param imageView     The image view in which the icon will be presented.
     * @param defaultIcon   The default icon that will be displayed if not selected.
     * @param isSelected    Whether the item is selected or not.
     */
    public static void applyModernIconStyle(
            ImageView imageView, Drawable defaultIcon, boolean isSelected) {
        imageView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        imageView.setImageDrawable(
                isSelected ? TintedDrawable.constructTintedDrawable(imageView.getContext(),
                        R.drawable.ic_check_googblue_24dp, R.color.default_icon_color_inverse)
                           : defaultIcon);
        imageView.getBackground().setLevel(isSelected
                        ? imageView.getResources().getInteger(R.integer.list_item_level_selected)
                        : imageView.getResources().getInteger(R.integer.list_item_level_default));
    }
}
