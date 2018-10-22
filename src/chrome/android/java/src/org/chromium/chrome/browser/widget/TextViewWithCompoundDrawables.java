// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.AppCompatTextView;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * A {@link TextView} with support for explicitly sizing and tinting compound drawables.
 *
 * To specify the drawable size, use the {@code drawableWidth} and {@code drawableHeight}
 * attributes. To specify the drawable tint, use the {@code chromeDrawableTint} attribute.
 */
public class TextViewWithCompoundDrawables extends AppCompatTextView {
    private int mDrawableWidth;
    private int mDrawableHeight;
    private ColorStateList mDrawableTint;

    public TextViewWithCompoundDrawables(Context context) {
        this(context, null);
    }

    public TextViewWithCompoundDrawables(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public TextViewWithCompoundDrawables(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs, defStyleAttr);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();

        if (mDrawableTint != null) {
            setDrawableTint(ApiCompatibilityUtils.getCompoundDrawablesRelative(this));
        }
    }

    private void init(Context context, AttributeSet attrs, int defStyleAttr) {
        TypedArray array = context.obtainStyledAttributes(
                attrs, R.styleable.TextViewWithCompoundDrawables, defStyleAttr, 0);

        mDrawableWidth = array.getDimensionPixelSize(
                R.styleable.TextViewWithCompoundDrawables_drawableWidth, -1);
        mDrawableHeight = array.getDimensionPixelSize(
                R.styleable.TextViewWithCompoundDrawables_drawableHeight, -1);
        mDrawableTint = array.getColorStateList(
                R.styleable.TextViewWithCompoundDrawables_chromeDrawableTint);

        array.recycle();

        if (mDrawableWidth <= 0 && mDrawableHeight <= 0 && mDrawableTint == null) return;

        Drawable[] drawables = ApiCompatibilityUtils.getCompoundDrawablesRelative(this);
        for (Drawable drawable : drawables) {
            if (drawable == null) continue;

            if (mDrawableWidth > 0 || mDrawableHeight > 0) {
                Rect bounds = drawable.getBounds();
                if (mDrawableWidth > 0) {
                    bounds.right = bounds.left + Math.round(mDrawableWidth);
                }
                if (mDrawableHeight > 0) {
                    bounds.bottom = bounds.top + Math.round(mDrawableHeight);
                }
                drawable.setBounds(bounds);
            }
        }

        if (mDrawableTint != null) setDrawableTint(drawables);

        ApiCompatibilityUtils.setCompoundDrawablesRelative(
                this, drawables[0], drawables[1], drawables[2], drawables[3]);
    }

    private void setDrawableTint(Drawable[] drawables) {
        for (Drawable drawable : drawables) {
            if (drawable == null) continue;

            drawable.setColorFilter(
                    mDrawableTint.getColorForState(getDrawableState(), 0), PorterDuff.Mode.SRC_IN);
        }
    }
}
