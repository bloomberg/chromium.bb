// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;

/** {@link ImageView} which always aspect-ratio scales the image to fit its container. */
public class AspectRatioScalingImageView extends ImageView {
    /** Default aspect ratio for the view when the drawable is not set. Set to zero to ignore. */
    private float mDefaultAspectRatio;

    public AspectRatioScalingImageView(Context context) {
        super(context);
    }

    public void setDefaultAspectRatio(float aspectRatio) {
        mDefaultAspectRatio = aspectRatio;
        invalidate();
    }

    /**
     * This custom onMeasure scales the image to fill the container. If the container has only one
     * constrained dimension, the image is aspect ratio scaled to its max possible size given the
     * constraining dimension.
     *
     * <p>This is overridden because adjustViewBounds does not scale up small images in API 17-, and
     * because we want the image to scale independent of the dimensions of the Drawable - the image
     * should not change size based on the resolution of the Drawable, only the aspect ratio.
     */
    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        Drawable drawable = getDrawable();

        // Try to get the aspect ratio; if we can't, fall back to super.
        // width / height
        float aspectRatio;
        if (drawable != null && drawable.getIntrinsicHeight() > 0
                && drawable.getIntrinsicWidth() > 0) {
            aspectRatio = ((float) drawable.getIntrinsicWidth()) / drawable.getIntrinsicHeight();
        } else if (mDefaultAspectRatio > 0) {
            aspectRatio = mDefaultAspectRatio;
        } else {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }
        // height / width
        float inverseAspectRatio = 1 / aspectRatio;

        int widthSpecMode = MeasureSpec.getMode(widthMeasureSpec);
        int heightSpecMode = MeasureSpec.getMode(heightMeasureSpec);

        int measuredWidth = MeasureSpec.getSize(widthMeasureSpec);
        int measuredHeight = MeasureSpec.getSize(heightMeasureSpec);

        if (widthSpecMode == MeasureSpec.UNSPECIFIED && heightSpecMode != MeasureSpec.UNSPECIFIED) {
            // Aspect ratio scale the width
            measuredWidth = aspectRatioScaleWidth(measuredHeight, aspectRatio);
        } else if (heightSpecMode == MeasureSpec.UNSPECIFIED
                && widthSpecMode != MeasureSpec.UNSPECIFIED) {
            // Aspect ratio scale the height
            measuredHeight = aspectRatioScaleHeight(measuredWidth, inverseAspectRatio);
        } else if (heightSpecMode == MeasureSpec.UNSPECIFIED
                && widthSpecMode == MeasureSpec.UNSPECIFIED) {
            // If both are UNSPECIFIED, take up as much room as possible.
            measuredWidth = Integer.MAX_VALUE;
            measuredHeight = Integer.MAX_VALUE;
        } else if (widthSpecMode == MeasureSpec.AT_MOST && heightSpecMode == MeasureSpec.EXACTLY) {
            measuredWidth =
                    Math.min(aspectRatioScaleWidth(measuredHeight, aspectRatio), measuredWidth);
        } else if (heightSpecMode == MeasureSpec.AT_MOST && widthSpecMode == MeasureSpec.EXACTLY) {
            measuredHeight = Math.min(
                    aspectRatioScaleHeight(measuredWidth, inverseAspectRatio), measuredHeight);
        } else if (widthSpecMode == MeasureSpec.AT_MOST && heightSpecMode == MeasureSpec.AT_MOST) {
            int desiredWidth = aspectRatioScaleWidth(measuredHeight, aspectRatio);
            int desiredHeight = aspectRatioScaleHeight(measuredWidth, inverseAspectRatio);
            if (desiredWidth < measuredWidth) {
                measuredWidth = desiredWidth;
            } else if (desiredHeight < measuredHeight) {
                measuredHeight = desiredHeight;
            }
        }
        // else keep values from the MeasureSpec because both modes are EXACTLY.

        setMeasuredDimension(measuredWidth, measuredHeight);
    }

    private int aspectRatioScaleWidth(int constrainingHeight, float aspectRatio) {
        int imageHeight = constrainingHeight - getPaddingTop() - getPaddingBottom();
        int imageWidth = (int) (imageHeight * aspectRatio);
        return imageWidth + getPaddingRight() + getPaddingLeft();
    }

    private int aspectRatioScaleHeight(int constrainingWidth, float inverseAspectRatio) {
        int imageWidth = constrainingWidth - getPaddingRight() - getPaddingLeft();
        int imageHeight = (int) (imageWidth * inverseAspectRatio);
        return imageHeight + getPaddingTop() + getPaddingBottom();
    }
}
