// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.ProgressBar;

import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.touchless.R;

/**
 * A pill shaped progress bar. This view is blank for the most part, except for the bottom
 * R.dimen.notouch_progressbar_fill_height where the progress is shown.
 */
@TargetApi(Build.VERSION_CODES.M)
public class PillProgressBarView extends ProgressBar {
    // The start and the end of the progress bar will be hidden behind the corner radius. This
    // variable holds the offset progress that is hidden.
    private int mDispalyableProgressOffset;

    // ClipDrawable's max is a fixed constant 10000.
    // http://developer.android.com/reference/android/graphics/drawable/ClipDrawable.html
    private static final int CLIP_DRAWABLE_MAX = 10000;

    public PillProgressBarView(Context context, AttributeSet attrs) {
        super(context, attrs);
        Drawable pillDrawable = getResources().getDrawable(R.drawable.notouch_progress_bar_pill);
        setBackground(pillDrawable);
        setForeground(new ClipDrawable(pillDrawable, Gravity.TOP, ClipDrawable.VERTICAL));
        setProgressDrawable(getResources().getDrawable(R.drawable.notouch_progress_bar_drawable));
    }

    @Override
    protected synchronized void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        // The height of the portion of the progress bar that is visible.
        float progressFillHeight =
                getResources().getDimension(R.dimen.notouch_progressbar_fill_height);
        // The radius of pill's curved corners.
        float radius = getResources().getDimension(R.dimen.notouch_progressbar_radius);

        // Clip the white foreground to only show progressFillHeight of progress bar at the bottom.
        int clipLevel = (int) (CLIP_DRAWABLE_MAX * (getMeasuredHeight() - progressFillHeight)
                / getMeasuredHeight());
        getForeground().setLevel(clipLevel);

        // Because of the corner radius, a portion of the progress bar is obscured from left and
        // right by the white overlay. We should calculate this offset in order to scale the
        // progress value accordingly.
        float dispalyableProgressOffsetPx = radius
                - (float) Math.sqrt(Math.pow(radius, 2) - Math.pow(radius - progressFillHeight, 2));
        mDispalyableProgressOffset =
                (int) ((dispalyableProgressOffsetPx * getMax()) / getMeasuredWidth());
    }

    // public synchronized methods cause a lint warning, but this needs to be synchronized because
    // of the super method.
    @SuppressLint("NoSynchronizedMethodCheck")
    @Override
    public synchronized void setProgress(int progress) {
        // Scale progress from [0, getMax()] to
        // [mDispalyableProgressOffset, getMax() - mDispalyableProgressOffset].
        int scaledProgress = (int) MathUtils.map(progress, 0, getMax(), mDispalyableProgressOffset,
                getMax() - mDispalyableProgressOffset);
        super.setProgress(scaledProgress);
    }
}
