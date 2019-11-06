// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Draws the overlay background, possibly leaving out some portions visible. Supports scrolling.
 *
 * <p>This drawable takes a transparent area, which corresponds to the position of DOM elements on
 * the visible viewport.
 *
 * <p>While scrolling, it keeps track of the current scrolling offset and avoids drawing on top of
 * the top bar which is can be, during animations, just drawn on top of the compositor.
 */
class AssistantOverlayDrawable
        extends Drawable implements FullscreenListener, CompositorViewResizer.Observer {
    private static final int FADE_DURATION_MS = 250;

    /** Default background color and alpha. */
    private static final int DEFAULT_BACKGROUND_COLOR = Color.argb(0x42, 0, 0, 0);

    /** Width of the line drawn around the boxes. */
    private static final int BOX_STROKE_WIDTH_DP = 2;

    /** Padding added to boxes. */
    private static final int BOX_PADDING_DP = 2;

    /** Box corner. */
    private static final int BOX_CORNER_DP = 8;

    private final Context mContext;
    private final ChromeFullscreenManager mFullscreenManager;

    /**
     * The {@link CompositorViewResizer} associated to the Autofill Assistant BottomSheet content.
     */
    private final CompositorViewResizer mViewResizer;

    private final Paint mBackground;
    private int mBackgroundAlpha;
    private final Paint mBoxStroke;
    private int mBoxStrokeAlpha;
    private final Paint mBoxClear;
    private final Paint mBoxFill;

    /** When in partial mode, don't draw on {@link #mTransparentArea}. */
    private boolean mPartial;

    /**
     * Coordinates of the visual viewport within the page, if known, in CSS pixels relative to the
     * origin of the page.
     *
     * The visual viewport includes the portion of the page that is really visible, excluding any
     * area not fully visible because of the current zoom value.
     *
     * Only relevant in partial mode, when the transparent area is non-empty.
     */
    private final RectF mVisualViewport = new RectF();

    private final List<Box> mTransparentArea = new ArrayList<>();

    /** Padding added between the element area and the grayed-out area. */
    private final float mPaddingPx;

    /** Size of the corner of the cleared-out areas. */
    private final float mCornerPx;

    /** A single RectF instance used for drawing, to avoid creating many instances when drawing. */
    private final RectF mDrawRect = new RectF();

    /**
     * Current top margin of this view.
     *
     * <p>Margins are set when the top or bottom controller are fully shown. When they're shown
     * partially, during a scroll, margins are always 0. The drawing takes care of adapting.
     *
     * <p>Always 0 unless accessibility is turned on.
     *
     * <p>TODO(crbug.com/806868): Better integrate this filter with the view layout to make it
     * automatic.
     */
    private int mMarginTop;

    /** Current bottom margin of this view. */
    private int mMarginBottom;

    private AssistantOverlayDelegate mDelegate;

    AssistantOverlayDrawable(Context context, ChromeFullscreenManager fullscreenManager,
            CompositorViewResizer viewResizer) {
        mContext = context;
        mFullscreenManager = fullscreenManager;
        mViewResizer = viewResizer;

        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();

        mBackground = new Paint();
        mBackground.setStyle(Paint.Style.FILL);

        mBoxClear = new Paint();
        mBoxClear.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        mBoxClear.setColor(Color.BLACK);
        mBoxClear.setStyle(Paint.Style.FILL);

        mBoxFill = new Paint();
        mBoxFill.setStyle(Paint.Style.FILL);

        mBoxStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBoxStroke.setStyle(Paint.Style.STROKE);
        mBoxStroke.setStrokeWidth(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, BOX_STROKE_WIDTH_DP, displayMetrics));
        mBoxStroke.setStrokeCap(Paint.Cap.ROUND);

        mPaddingPx = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, BOX_PADDING_DP, displayMetrics);
        mCornerPx = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, BOX_CORNER_DP, displayMetrics);

        mFullscreenManager.addListener(this);
        mViewResizer.addObserver(this);

        // Sets colors to default.
        setBackgroundColor(null);
        setHighlightBorderColor(null);
    }

    /** Sets the overlay color or {@code null} to use the default color. */
    void setBackgroundColor(@Nullable Integer color) {
        if (color == null) {
            color = DEFAULT_BACKGROUND_COLOR;
        }
        mBackgroundAlpha = Color.alpha(color);
        mBackground.setColor(color);
        mBoxFill.setColor(color);
        invalidateSelf();
    }

    /** Sets the color of the border or {@code null} to use the default color. */
    void setHighlightBorderColor(@Nullable Integer color) {
        if (color == null) {
            color = ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.modern_blue_600);
        }
        mBoxStrokeAlpha = Color.alpha(color);
        mBoxStroke.setColor(color);
        invalidateSelf();
    }

    void setDelegate(AssistantOverlayDelegate delegate) {
        mDelegate = delegate;
    }

    void destroy() {
        mFullscreenManager.removeListener(this);
        mViewResizer.removeObserver(this);
        mDelegate = null;
    }

    /**
     * Set the current state of the overlay.
     */
    void setPartial(boolean partial) {
        if (partial == mPartial) return;

        if (partial) {
            // Transition from full to partial
            for (Box box : mTransparentArea) {
                box.fadeOut();
            }
        } else {
            // Transition from partial to full.
            for (Box box : mTransparentArea) {
                box.fadeIn();
            }
        }
        mPartial = partial;
        invalidateSelf();
    }

    void setVisualViewport(RectF visualViewport) {
        mVisualViewport.set(visualViewport);
        invalidateSelf();
    }

    /** Set or updates the transparent area. */
    void setTransparentArea(List<RectF> transparentArea) {
        // Add or update boxes for each rectangle in the area.
        for (int i = 0; i < transparentArea.size(); i++) {
            while (i >= mTransparentArea.size()) {
                mTransparentArea.add(new Box());
            }
            Box box = mTransparentArea.get(i);
            RectF rect = transparentArea.get(i);
            boolean isNew = box.mRect.isEmpty() && !rect.isEmpty();
            box.mRect.set(rect);
            if (mPartial && isNew) {
                // This box just appeared, fade it out of the background.
                box.fadeOut();
            }
            // Not fading in here as the element has likely already disappeared; the fade in could
            // end up on an unrelated portion of the page.
        }

        // Remove rectangles now gone from the area. Fading in works here, because the removal is
        // due to a script decision; the elements are still there.
        for (Iterator<Box> iter = mTransparentArea.listIterator(transparentArea.size());
                iter.hasNext();) {
            Box box = iter.next();
            if (!box.mRect.isEmpty()) {
                // Fade in rectangle.
                box.fadeIn();
                box.mRect.setEmpty();
            } else if (box.mAnimationType == AnimationType.NONE) {
                // We're done fading in. Cleanup.
                iter.remove();
            }
        }

        invalidateSelf();
    }

    @Override
    public void setAlpha(int alpha) {
        // Alpha is ignored.
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {}

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    /** Returns the origin of the visual viewport in this view. */
    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        int width = bounds.width();
        int yBottom = bounds.height()
                - (int) (mFullscreenManager.getBottomControlsHeight() + mViewResizer.getHeight()
                        - mFullscreenManager.getBottomControlOffset());

        // Don't draw over the top or bottom bars.
        canvas.clipRect(
                0, mFullscreenManager.getTopVisibleContentOffset() - mMarginTop, width, yBottom);

        canvas.drawPaint(mBackground);

        if (mVisualViewport.isEmpty()) return;

        // Ratio of to use to convert zoomed CSS pixels, to physical pixels. Aspect ratio is
        // conserved, so width and height are always converted with the same value. Using width
        // here, since viewport width always corresponds to the overlay width.
        float cssPixelsToPhysical = ((float) width) / ((float) mVisualViewport.width());

        int yTop = (int) mFullscreenManager.getContentOffset();
        for (Box box : mTransparentArea) {
            RectF rect = box.getRectToDraw();
            if (rect.isEmpty() || (!mPartial && box.mAnimationType != AnimationType.FADE_IN)) {
                continue;
            }
            // At visibility=1, stroke is fully opaque and box fill is fully transparent
            mBoxStroke.setAlpha((int) (mBoxStrokeAlpha * box.getVisibility()));
            int fillAlpha = (int) (mBackgroundAlpha * (1f - box.getVisibility()));
            mBoxFill.setAlpha(fillAlpha);

            mDrawRect.left = (rect.left - mVisualViewport.left) * cssPixelsToPhysical - mPaddingPx;
            mDrawRect.top =
                    yTop + (rect.top - mVisualViewport.top) * cssPixelsToPhysical - mPaddingPx;
            mDrawRect.right =
                    (rect.right - mVisualViewport.left) * cssPixelsToPhysical + mPaddingPx;
            mDrawRect.bottom =
                    yTop + (rect.bottom - mVisualViewport.top) * cssPixelsToPhysical + mPaddingPx;
            if (mDrawRect.left <= 0 && mDrawRect.right >= width) {
                // Rounded corners look strange in the case where the rectangle takes exactly the
                // width of the screen.
                canvas.drawRect(mDrawRect, mBoxClear);
                if (fillAlpha > 0) canvas.drawRect(mDrawRect, mBoxFill);
                canvas.drawRect(mDrawRect, mBoxStroke);
            } else {
                canvas.drawRoundRect(mDrawRect, mCornerPx, mCornerPx, mBoxClear);
                if (fillAlpha > 0) canvas.drawRoundRect(mDrawRect, mCornerPx, mCornerPx, mBoxFill);
                canvas.drawRoundRect(mDrawRect, mCornerPx, mCornerPx, mBoxStroke);
            }
        }
    }

    @Override
    public void onContentOffsetChanged(int offset) {
        invalidateSelf();
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int bottomOffset, boolean needsAnimate) {
        invalidateSelf();
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {
        invalidateSelf();
    }

    @Override
    public void onUpdateViewportSize() {
        askForTouchableAreaUpdate();
        invalidateSelf();
    }

    @Override
    public void onHeightChanged(int height) {
        askForTouchableAreaUpdate();
        invalidateSelf();
    }

    private void askForTouchableAreaUpdate() {
        if (mDelegate != null) {
            mDelegate.updateTouchableArea();
        }
    }

    @IntDef({AnimationType.NONE, AnimationType.FADE_IN, AnimationType.FADE_OUT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface AnimationType {
        int NONE = 0;
        int FADE_IN = 1;
        int FADE_OUT = 2;
    }

    private class Box {
        /** Current rectangle and touchable area, as reported by the model. */
        final RectF mRect = new RectF();

        /** A copy of the rectangle that used to be displayed in mRect while fading in. */
        @Nullable
        RectF mFadeInRect;

        /** Type of {@link #mAnimator}. */
        @AnimationType
        int mAnimationType = AnimationType.NONE;

        /** Current animation. Cleared at end of animation. */
        @Nullable
        ValueAnimator mAnimator;

        /**
         * Returns 0 if the box should be invisible, 1 if it should be fully visible.
         *
         * <p>A fully visible box is transparent. An invisible box is identical to the background.
         */
        float getVisibility() {
            return mAnimator != null ? (float) mAnimator.getAnimatedValue() : 1f;
        }

        /** Returns the rectangle that should be drawn. */
        RectF getRectToDraw() {
            return mFadeInRect != null ? mFadeInRect : mRect;
        }

        /** Fades out to the current rectangle. Does nothing if empty or already fading out. */
        void fadeOut() {
            if (!setupAnimator(
                        AnimationType.FADE_OUT, 0f, 1f, BakedBezierInterpolator.FADE_OUT_CURVE)) {
                return;
            }
            mAnimator.start();
        }

        /** Fades the current rectangle in. Does nothing if empty or already fading in. */
        void fadeIn() {
            if (!setupAnimator(
                        AnimationType.FADE_IN, 1f, 0f, BakedBezierInterpolator.FADE_IN_CURVE)) {
                return;
            }
            mFadeInRect = new RectF(mRect);
            mAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator ignored) {
                    mFadeInRect = null;
                }

                @Override
                public void onAnimationCancel(Animator ignored) {
                    mFadeInRect = null;
                }
            });
            mAnimator.start();
        }

        boolean setupAnimator(@AnimationType int animationType, float start, float end,
                TimeInterpolator interpolator) {
            if (mRect.isEmpty()) {
                return false;
            }

            if (mAnimator != null && mAnimator.isRunning()) {
                if (mAnimationType == animationType) {
                    return false;
                }
                start = (Float) mAnimator.getAnimatedValue();
                mAnimator.cancel();
            }
            mAnimationType = animationType;
            mAnimator = ValueAnimator.ofFloat(start, end);
            mAnimator.setDuration(FADE_DURATION_MS);
            mAnimator.setInterpolator(interpolator);
            mAnimator.addUpdateListener((ignoredAnimator) -> invalidateSelf());
            mAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator ignored) {
                    mAnimationType = AnimationType.NONE;
                    mAnimator = null;
                }
            });
            return true;
        }
    }
}
