// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.os.SystemClock;

/** Helper class for displaying the press feedback animations. */
public final class FeedbackAnimator
        implements Event.ParameterCallback<Boolean, PaintEventParameter> {
    /** Total duration of the animation, in milliseconds. */
    private static final float TOTAL_DURATION_MS = 220;

    /** Start time of the animation, from {@link SystemClock#uptimeMillis()}. */
    private final long mStartTimeInMs;

    /** Contains the size of the feedback animation for the most recent request. */
    private final float mFeedbackSizeInPixels;

    private final Paint mPaint = new Paint();

    private final Point mPos;

    private FeedbackAnimator(float feedbackSizeInPixels, Point pos) {
        mStartTimeInMs = SystemClock.uptimeMillis();
        mFeedbackSizeInPixels = feedbackSizeInPixels;
        mPos = pos;
    }

    /** Begins a new animation sequence at position (|pos|). */
    public static void startAnimation(DesktopViewInterface view,
                                      Point pos,
                                      DesktopView.InputFeedbackType feedbackType) {
        if (feedbackType == DesktopView.InputFeedbackType.NONE) {
            return;
        }

        view.onPaint().addSelfRemovable(new FeedbackAnimator(
                getInputFeedbackSizeInPixels(feedbackType), pos));
    }

    private static float getInputFeedbackSizeInPixels(DesktopView.InputFeedbackType feedbackType) {
        switch (feedbackType) {
            case SMALL_ANIMATION:
                return 40.0f;

            case LARGE_ANIMATION:
                return 160.0f;

            default:
                // Unreachable, but required by Google Java style and findbugs.
                assert false : "Unreached";
                return 0.0f;
        }
    }

    @Override
    public Boolean run(PaintEventParameter parameter) {
        float elapsedTimeInMs = SystemClock.uptimeMillis() - mStartTimeInMs;
        if (elapsedTimeInMs < 1) {
            return true;
        }

        // |progress| is 0 at the beginning, 1 at the end.
        float progress = elapsedTimeInMs / TOTAL_DURATION_MS;
        if (progress >= 1) {
            return false;
        }

        float size = mFeedbackSizeInPixels / parameter.scale;

        // Animation grows from 0 to |size|, and goes from fully opaque to transparent for a
        // seamless fading-out effect. The animation needs to have more than one color so it's
        // visible over any background color.
        float radius = size * progress;
        int alpha = (int) ((1 - progress) * 0xff);

        int transparentBlack = Color.argb(0, 0, 0, 0);
        int white = Color.argb(alpha, 0xff, 0xff, 0xff);
        int black = Color.argb(alpha, 0, 0, 0);
        mPaint.setShader(new RadialGradient(mPos.x, mPos.y, radius,
                new int[] {transparentBlack, white, black, transparentBlack},
                new float[] {0.0f, 0.8f, 0.9f, 1.0f}, Shader.TileMode.CLAMP));
        parameter.canvas.drawCircle(mPos.x, mPos.y, radius, mPaint);
        return true;
    }
}
