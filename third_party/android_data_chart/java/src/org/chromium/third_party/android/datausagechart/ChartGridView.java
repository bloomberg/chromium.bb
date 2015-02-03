/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.chromium.third_party.android.datausagechart;

import static android.text.format.DateUtils.FORMAT_ABBREV_MONTH;
import static android.text.format.DateUtils.FORMAT_SHOW_DATE;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.text.format.DateUtils;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.third_party.android.R;

import java.util.Locale;

/**
 * Background of {@link ChartView} that renders grid lines as requested by
 * {@link ChartAxis#getTickPoints()}.
 * This is derived from com.android.settings.widget.ChartGridView.
 */
public class ChartGridView extends View {

    private ChartAxis mHoriz;
    private ChartAxis mVert;

    private Drawable mPrimary;
    private Drawable mSecondary;
    private Drawable mBorder;

    public static String formatDateRange(Context context, long start, long end) {
        final int flags = FORMAT_SHOW_DATE | FORMAT_ABBREV_MONTH;
        final StringBuilder builder = new StringBuilder(50);
        final java.util.Formatter formatter = new java.util.Formatter(
                builder, Locale.getDefault());
        return DateUtils.formatDateRange(context, formatter, start, end, flags, null).toString();
    }

    public ChartGridView(Context context) {
        this(context, null, 0);
    }

    public ChartGridView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ChartGridView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        setWillNotDraw(false);

        final TypedArray a = context.obtainStyledAttributes(
                attrs, R.styleable.ChartGridView, defStyle, 0);

        mPrimary = a.getDrawable(R.styleable.ChartGridView_primaryDrawable);
        mSecondary = a.getDrawable(R.styleable.ChartGridView_secondaryDrawable);
        mBorder = a.getDrawable(R.styleable.ChartGridView_borderDrawable);
        a.recycle();
    }

    void init(ChartAxis horiz, ChartAxis vert) {
        if (horiz == null) throw new NullPointerException("missing horiz");
        if (vert == null) throw new NullPointerException("missing vert");
        mHoriz = horiz;
        mVert = vert;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        final int width = getWidth();
        final int height = getHeight();

        final Drawable secondary = mSecondary;
        float density = getResources().getDisplayMetrics().density;
        final int secondaryHeight = Math.max(1, Math.round(density));

        final float[] vertTicks = mVert.getTickPoints();
        for (float y : vertTicks) {
            final int bottom = (int) Math.min(y + secondaryHeight, height);
            secondary.setBounds(0, (int) y, width, bottom);
            secondary.draw(canvas);
        }

        final Drawable primary = mPrimary;
        final int primaryWidth = secondaryHeight;

        final float[] horizTicks = mHoriz.getTickPoints();
        for (float x : horizTicks) {
            final int right = (int) Math.min(x + primaryWidth, width);
            primary.setBounds((int) x, 0, right, height);
            primary.draw(canvas);
        }

        mBorder.setBounds(0, 0, width, height);
        mBorder.draw(canvas);
    }
}