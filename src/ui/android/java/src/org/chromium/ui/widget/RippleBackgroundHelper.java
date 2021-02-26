// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RippleDrawable;
import android.util.StateSet;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.ColorUtils;
import androidx.core.view.ViewCompat;

import org.chromium.ui.R;

/**
 * A helper class to create and maintain a background drawable with customized background color,
 * ripple color, and corner radius.
 */
// TODO(jdemeulenaere): Make this class package-private once it is not accessed by {@link
// org.chromium.chrome.browser.autofill_assistant.carousel.ButtonView} anymore.
public class RippleBackgroundHelper {
    private static final int[] STATE_SET_PRESSED = {android.R.attr.state_pressed};
    private static final int[] STATE_SET_SELECTED = {android.R.attr.state_selected};
    private static final int[] STATE_SET_SELECTED_PRESSED = {
            android.R.attr.state_selected, android.R.attr.state_pressed};

    private final View mView;

    private @Nullable ColorStateList mBackgroundColorList;

    private GradientDrawable mBackgroundGradient;

    // Used for applying tint on pre-L versions.
    private Drawable mBackgroundDrawablePreL;
    private Drawable mRippleDrawablePreL;
    private Drawable mBorderDrawablePreL;

    /**
     * @param view The {@link View} on which background will be applied.
     * @param backgroundColorResId The resource id of the background color.
     * @param rippleColorResId The resource id of the ripple color.
     * @param cornerRadius The corner radius in pixels of the background drawable.
     * @param verticalInset The vertical inset of the background drawable.
     */
    RippleBackgroundHelper(View view, @ColorRes int backgroundColorResId,
            @ColorRes int rippleColorResId, @Px int cornerRadius, @Px int verticalInset) {
        this(view, backgroundColorResId, rippleColorResId, cornerRadius,
                android.R.color.transparent, R.dimen.default_ripple_background_border_size,
                verticalInset);
    }

    /**
     * @param view The {@link View} on which background will be applied.
     * @param backgroundColorResId The resource id of the background color.
     * @param rippleColorResId The resource id of the ripple color.
     * @param cornerRadius The corner radius in pixels of the background drawable.
     * @param borderColorResId The resource id of the border color.
     * @param borderSizeDimenId The resource id of the border size.
     * @param verticalInset The vertical inset of the background drawable.
     */
    // TODO(jdemeulenaere): Make this constructor package-private once it is not accessed by {@link
    // org.chromium.chrome.browser.autofill_assistant.carousel.ButtonView} anymore.
    public RippleBackgroundHelper(View view, @ColorRes int backgroundColorResId,
            @ColorRes int rippleColorResId, @Px int cornerRadius, @ColorRes int borderColorResId,
            @DimenRes int borderSizeDimenId, @Px int verticalInset) {
        mView = view;

        int paddingStart = ViewCompat.getPaddingStart(mView);
        int paddingTop = mView.getPaddingTop();
        int paddingEnd = ViewCompat.getPaddingEnd(mView);
        int paddingBottom = mView.getPaddingBottom();
        mView.setBackground(createBackgroundDrawable(
                AppCompatResources.getColorStateList(view.getContext(), rippleColorResId),
                AppCompatResources.getColorStateList(view.getContext(), borderColorResId),
                view.getResources().getDimensionPixelSize(borderSizeDimenId), cornerRadius,
                verticalInset));
        setBackgroundColor(
                AppCompatResources.getColorStateList(view.getContext(), backgroundColorResId));
    }

    /**
     * This initializes all members with new drawables needed to display/update a ripple effect.
     * @param rippleColorList A {@link ColorStateList} that is used for the ripple effect.
     * @param borderColorList A {@link ColorStateList} that is used for the border.
     * @param borderSize The border width in pixels.
     * @param cornerRadius The corner radius in pixels.
     * @param verticalInset The vertical inset of the background drawable.
     * @return The {@link GradientDrawable}/{@link LayerDrawable} to be used as ripple background.
     */
    private Drawable createBackgroundDrawable(ColorStateList rippleColorList,
            ColorStateList borderColorList, @Px int borderSize, @Px int cornerRadius,
            @Px int verticalInset) {
        mBackgroundGradient = new GradientDrawable();
        mBackgroundGradient.setCornerRadius(cornerRadius);
        if (borderSize > 0) mBackgroundGradient.setStroke(borderSize, borderColorList);
        GradientDrawable mask = new GradientDrawable();
        mask.setCornerRadius(cornerRadius);
        mask.setColor(Color.WHITE);
        return wrapDrawableWithInsets(
                new RippleDrawable(convertToRippleDrawableColorList(rippleColorList),
                        mBackgroundGradient, mask),
                verticalInset);
    }

    /**
     * @param drawable The {@link Drawable} that needs to be wrapped with insets.
     * @param verticalInset The vertical inset for the specified drawable.
     * @return A {@link Drawable} that wraps the specified drawable with the specified inset.
     */
    private static Drawable wrapDrawableWithInsets(Drawable drawable, @Px int verticalInset) {
        if (verticalInset == 0) return drawable;
        return new InsetDrawable(drawable, 0, verticalInset, 0, verticalInset);
    }

    /**
     * @param color The {@link ColorStateList} to be set as the background color on the background
     *              drawable.
     */
    void setBackgroundColor(ColorStateList color) {
        if (color == mBackgroundColorList) return;

        mBackgroundColorList = color;
        mBackgroundGradient.setColor(color);
    }

    /**
     * @return The color under the specified states in the specified {@link ColorStateList}.
     */
    private static @ColorInt int getColorForState(ColorStateList colorStateList, int[] states) {
        return colorStateList.getColorForState(states, colorStateList.getDefaultColor());
    }

    /**
     * Adjusts the opacity of the ripple color since {@link RippleDrawable} uses about 50% opacity
     * of color for ripple effect.
     */
    private @ColorInt static int doubleAlpha(@ColorInt int color) {
        int alpha = Math.min(Color.alpha(color) * 2, 255);
        return ColorUtils.setAlphaComponent(color, alpha);
    }

    /**
     * Converts the specified {@link ColorStateList} to one that can be applied to a
     * {@link RippleDrawable}.
     */
    private static ColorStateList convertToRippleDrawableColorList(ColorStateList colorStateList) {
        return new ColorStateList(new int[][] {STATE_SET_SELECTED, StateSet.NOTHING},
                new int[] {
                        doubleAlpha(getColorForState(colorStateList, STATE_SET_SELECTED_PRESSED)),
                        doubleAlpha(getColorForState(colorStateList, STATE_SET_PRESSED))});
    }
}
