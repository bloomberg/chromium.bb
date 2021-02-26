/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

package org.skia.skottie;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.net.Uri;
import android.util.AttributeSet;
import android.view.SurfaceView;
import android.view.TextureView;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import java.io.FileNotFoundException;
import java.io.InputStream;
import org.skia.skottie.SkottieRunner.SkottieAnimation;
import org.skia.skottielib.R;

public class SkottieView extends FrameLayout {

    private SkottieAnimation mAnimation;
    private View mBackingView;
    private int mBackgroundColor;
    // Repeat follows Animator API, infinite is represented by -1 (see Animator.DURATION_INFINITE)
    private int mRepeatCount;

    private static final int BACKING_VIEW_TEXTURE = 0;
    private static final int BACKING_VIEW_SURFACE = 1;

    // SkottieView constructor when initialized in XML layout
    public SkottieView(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.getTheme()
            .obtainStyledAttributes(attrs, R.styleable.SkottieView, 0, 0);
        try {
            // set mRepeatCount
            mRepeatCount = a.getInteger(R.styleable.SkottieView_android_repeatCount, 0);
            // set backing view and background color
            switch (a.getInteger(R.styleable.SkottieView_backing_view, -1)) {
                case BACKING_VIEW_TEXTURE:
                    mBackingView = new TextureView(context);
                    ((TextureView) mBackingView).setOpaque(false);
                    mBackgroundColor = a.getColor(R.styleable.SkottieView_background_color, 0);
                    break;
                case BACKING_VIEW_SURFACE:
                    mBackingView = new SurfaceView(context);
                    mBackgroundColor = a.getColor(R.styleable.SkottieView_background_color, -1);
                    if (mBackgroundColor == -1) {
                        throw new RuntimeException("background_color attribute "
                            + "needed for SurfaceView");
                    }
                    if (Color.alpha(mBackgroundColor) != 255) {
                        throw new RuntimeException("background_color cannot be transparent");
                    }
                    break;
                default:
                    throw new RuntimeException("backing_view attribute needed to "
                        + "specify between texture_view or surface_view");
            }
            // set source
            int src = a.getResourceId(R.styleable.SkottieView_src, -1);
            if (src != -1) {
                setSource(src);
            }
        } finally {
            a.recycle();
        }
        initBackingView();
    }
    
    // SkottieView builder constructor
    private SkottieView(Context context, SkottieViewBuilder builder) {
        super(context, builder.attrs, builder.defStyleAttr);
        // create the backing view
        if (builder.advancedFeatures) {
            // backing view must be SurfaceView
            mBackingView = new SurfaceView(context);
        } else {
            mBackingView = new TextureView(context);
            ((TextureView) mBackingView).setOpaque(false);
        }
        initBackingView();
    }

    private void initBackingView() {
        mBackingView.setLayoutParams(new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));
        addView(mBackingView);
    }

    public void setSource(InputStream inputStream) {
        mAnimation = setSourceHelper(inputStream);
    }

    public void setSource(int resId) {
        InputStream inputStream = mBackingView.getResources().openRawResource(resId);
        mAnimation = setSourceHelper(inputStream);
    }

    public void setSource(Context context, Uri uri) throws FileNotFoundException {
        InputStream inputStream = context.getContentResolver().openInputStream(uri);
        mAnimation = setSourceHelper(inputStream);
    }

    private SkottieAnimation setSourceHelper(InputStream inputStream) {
        if (mBackingView instanceof TextureView) {
            return SkottieRunner.getInstance()
                .createAnimation(((TextureView) mBackingView), inputStream, mBackgroundColor, mRepeatCount);
        } else {
            return SkottieRunner.getInstance()
                .createAnimation(((SurfaceView) mBackingView), inputStream, mBackgroundColor, mRepeatCount);
        }
    }

    protected SkottieAnimation getSkottieAnimation() {
        return mAnimation;
    }

    // progress: a float from 0 to 1 representing the percent into the animation
    public void seek(float progress) {
        mAnimation.setProgress(progress);
    }

    public void play() {
        mAnimation.resume();
    }

    public void pause() {
        mAnimation.pause();
    }

    public void start() {
        mAnimation.start();
    }

    public void stop() {
        mAnimation.end();
    }

    public float getProgress() {
        return mAnimation.getProgress();
    }

    // Builder accessed by user to generate SkottieViews
    public static class SkottieViewBuilder {
        protected AttributeSet attrs;
        protected int defStyleAttr;

        // if true, backing view will be surface view
        protected boolean advancedFeatures;
        // TODO private variable backgroundColor

        public void setAttrs(AttributeSet attrs) {
          this.attrs = attrs;
        }

        public void setDefStyleAttr(int defStyleAttr) {
          this.defStyleAttr = defStyleAttr;
        }

        public void setAdvancedFeatures(boolean advancedFeatures) {
          this.advancedFeatures = advancedFeatures;
        }

        public SkottieView build(Context context) {
          return new SkottieView(context, this);
        }
    }
}
