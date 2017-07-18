// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.content.Context;
import android.graphics.Point;
import android.view.Display;
import android.view.Surface;

import java.util.WeakHashMap;

/**
 * Chromium's object for android.view.Display. Instances of this object should be obtained
 * from WindowAndroid.
 * This class is designed to avoid leaks. It is ok to hold a strong ref of this class from
 * anywhere, as long as the corresponding WindowAndroids are destroyed. The observers are
 * held weakly so to not lead to leaks.
 */
public class DisplayAndroid {
    /**
     * DisplayAndroidObserver interface for changes to this Display.
     */
    public interface DisplayAndroidObserver {
        /**
         * Called whenever the screen orientation changes.
         *
         * @param orientation One of Surface.ROTATION_* values.
         */
        void onRotationChanged(int rotation);

        /**
         * Called whenever the screen density changes.
         *
         * @param screen density, aka Density Independent Pixel scale.
         */
        void onDIPScaleChanged(float dipScale);
    }

    private static final DisplayAndroidObserver[] EMPTY_OBSERVER_ARRAY =
            new DisplayAndroidObserver[0];

    private final WeakHashMap<DisplayAndroidObserver, Object /* null */> mObservers;
    // Do NOT add strong references to objects with potentially complex lifetime, like Context.

    private final int mDisplayId;
    private Point mSize;
    private float mDipScale;
    private int mBitsPerPixel;
    private int mBitsPerComponent;
    private int mRotation;
    protected boolean mIsDisplayWideColorGamut;
    protected boolean mIsDisplayServerWideColorGamut;

    protected static DisplayAndroidManager getManager() {
        return DisplayAndroidManager.getInstance();
    }

    /**
     * Get the non-multi-display DisplayAndroid for the given context. It's safe to call this with
     * any type of context, including the Application.
     *
     * To support multi-display, obtain DisplayAndroid from WindowAndroid instead.
     *
     * This function is intended to be analogous to GetPrimaryDisplay() for other platforms.
     * However, Android has historically had no real concept of a Primary Display, and instead uses
     * the notion of a default display for an Activity. Under normal circumstances, this function,
     * called with the correct context, will return the expected display for an Activity. However,
     * virtual, or "fake", displays that are not associated with any context may be used in special
     * cases, like Virtual Reality, and will lead to this function returning the incorrect display.
     *
     * @return What the Android WindowManager considers to be the default display for this context.
     */
    public static DisplayAndroid getNonMultiDisplay(Context context) {
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        return getManager().getDisplayAndroid(display);
    }

    /**
     * @return Display id that does not necessarily match the one defined in Android's Display.
     */
    public int getDisplayId() {
        return mDisplayId;
    }

    /**
     * @return Display height in physical pixels.
     */
    public int getDisplayHeight() {
        return mSize.y;
    }

    /**
     * @return Display width in physical pixels.
     */
    public int getDisplayWidth() {
        return mSize.x;
    }

    /**
     * @return current orientation. One of Surface.ORIENTATION_* values.
     */
    public int getRotation() {
        return mRotation;
    }

    /**
     * @return current orientation in degrees. One of the values 0, 90, 180, 270.
     */
    /* package */ int getRotationDegrees() {
        switch (getRotation()) {
            case Surface.ROTATION_0:
                return 0;
            case Surface.ROTATION_90:
                return 90;
            case Surface.ROTATION_180:
                return 180;
            case Surface.ROTATION_270:
                return 270;
        }

        // This should not happen.
        assert false;
        return 0;
    }

    /**
     * @return A scaling factor for the Density Independent Pixel unit.
     */
    public float getDipScale() {
        return mDipScale;
    }

    /**
     * @return Number of bits per pixel.
     */
    /* package */ int getBitsPerPixel() {
        return mBitsPerPixel;
    }

    /**
     * @return Number of bits per each color component.
     */
    @SuppressWarnings("deprecation")
    /* package */ int getBitsPerComponent() {
        return mBitsPerComponent;
    }

    /**
     * @return Whether or not it is possible to use wide color gamut rendering with this display.
     */
    public boolean getIsWideColorGamut() {
        return mIsDisplayWideColorGamut && mIsDisplayServerWideColorGamut;
    }

    /**
     * Add observer. Note repeat observers will be called only one.
     * Observers are held only weakly by Display.
     */
    public void addObserver(DisplayAndroidObserver observer) {
        mObservers.put(observer, null);
    }

    /**
     * Remove observer.
     */
    public void removeObserver(DisplayAndroidObserver observer) {
        mObservers.remove(observer);
    }

    /**
     * Toggle the accurate mode if it wasn't already doing so. The backend will
     * keep track of the number of times this has been called.
     */
    public static void startAccurateListening() {
        getManager().startAccurateListening();
    }

    /**
     * Request to stop the accurate mode. It will effectively be stopped only if
     * this method is called as many times as startAccurateListening().
     */
    public static void stopAccurateListening() {
        getManager().stopAccurateListening();
    }

    protected DisplayAndroid(int displayId) {
        mDisplayId = displayId;
        mObservers = new WeakHashMap<>();
        mSize = new Point();
    }

    private DisplayAndroidObserver[] getObservers() {
        // Makes a copy to allow concurrent edit.
        return mObservers.keySet().toArray(EMPTY_OBSERVER_ARRAY);
    }

    public void updateIsDisplayServerWideColorGamut(Boolean isDisplayServerWideColorGamut) {
        update(null, null, null, null, null, null, isDisplayServerWideColorGamut);
    }

    /**
     * Update the display to the provided parameters. Null values leave the parameter unchanged.
     */
    protected void update(Point size, Float dipScale, Integer bitsPerPixel,
            Integer bitsPerComponent, Integer rotation, Boolean isDisplayWideColorGamut,
            Boolean isDisplayServerWideColorGamut) {
        boolean sizeChanged = size != null && !mSize.equals(size);
        // Intentional comparison of floats: we assume that if scales differ, they differ
        // significantly.
        boolean dipScaleChanged = dipScale != null && mDipScale != dipScale;
        boolean bitsPerPixelChanged = bitsPerPixel != null && mBitsPerPixel != bitsPerPixel;
        boolean bitsPerComponentChanged =
                bitsPerComponent != null && mBitsPerComponent != bitsPerComponent;
        boolean rotationChanged = rotation != null && mRotation != rotation;
        boolean isDisplayWideColorGamutChanged = isDisplayWideColorGamut != null
                && mIsDisplayWideColorGamut != isDisplayWideColorGamut;
        boolean isDisplayServerWideColorGamutChanged = isDisplayServerWideColorGamut != null
                && mIsDisplayServerWideColorGamut != isDisplayServerWideColorGamut;

        boolean changed = sizeChanged || dipScaleChanged || bitsPerPixelChanged
                || bitsPerComponentChanged || rotationChanged || isDisplayWideColorGamutChanged
                || isDisplayServerWideColorGamutChanged;
        if (!changed) return;

        if (sizeChanged) mSize = size;
        if (dipScaleChanged) mDipScale = dipScale;
        if (bitsPerPixelChanged) mBitsPerPixel = bitsPerPixel;
        if (bitsPerComponentChanged) mBitsPerComponent = bitsPerComponent;
        if (rotationChanged) mRotation = rotation;
        if (isDisplayWideColorGamutChanged) mIsDisplayWideColorGamut = isDisplayWideColorGamut;
        if (isDisplayServerWideColorGamutChanged) {
            mIsDisplayServerWideColorGamut = isDisplayServerWideColorGamut;
        }

        getManager().updateDisplayOnNativeSide(this);
        if (rotationChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onRotationChanged(mRotation);
            }
        }
        if (dipScaleChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onDIPScaleChanged(mDipScale);
            }
        }
    }
}
