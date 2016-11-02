// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Point;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;

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
    }

    private static final String TAG = "DisplayAndroid";

    private static final DisplayAndroidObserver[] EMPTY_OBSERVER_ARRAY =
            new DisplayAndroidObserver[0];

    private final int mSdkDisplayId;
    private final WeakHashMap<DisplayAndroidObserver, Object /* null */> mObservers;
    // Do NOT add strong references to objects with potentially complex lifetime, like Context.

    // Updated by updateFromDisplay.
    private final Point mSize;
    private final Point mPhysicalSize;
    private final DisplayMetrics mDisplayMetrics;
    private int mRotation;

    // When this object exists, a positive value means that the forced DIP scale is set and
    // the zero means it is not. The non existing object (i.e. null reference) means that
    // the existence and value of the forced DIP scale has not yet been determined.
    private static Float sForcedDIPScale;

    private static boolean hasForcedDIPScale() {
        if (sForcedDIPScale == null) {
            String forcedScaleAsString = CommandLine.getInstance().getSwitchValue(
                    DisplaySwitches.FORCE_DEVICE_SCALE_FACTOR);
            if (forcedScaleAsString == null) {
                sForcedDIPScale = Float.valueOf(0.0f);
            } else {
                boolean isInvalid = false;
                try {
                    sForcedDIPScale = Float.valueOf(forcedScaleAsString);
                    // Negative values are discarded.
                    if (sForcedDIPScale.floatValue() <= 0.0f) isInvalid = true;
                } catch (NumberFormatException e) {
                    // Strings that do not represent numbers are discarded.
                    isInvalid = true;
                }

                if (isInvalid) {
                    Log.w(TAG, "Ignoring invalid forced DIP scale '" + forcedScaleAsString + "'");
                    sForcedDIPScale = Float.valueOf(0.0f);
                }
            }
        }
        return sForcedDIPScale.floatValue() > 0;
    }

    private static DisplayAndroidManager getManager() {
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
     * @return Real physical display height in physical pixels. Or 0 if not supported.
     */
    public int getPhysicalDisplayHeight() {
        return mPhysicalSize.y;
    }

    /**
     * @return Real physical display width in physical pixels. Or 0 if not supported.
     */
    public int getPhysicalDisplayWidth() {
        return mPhysicalSize.x;
    }

    /**
     * @return current orientation. One of Surface.ORIENTATION_* values.
     */
    public int getRotation() {
        return mRotation;
    }

    /**
     * @return A scaling factor for the Density Independent Pixel unit.
     */
    public float getDIPScale() {
        return mDisplayMetrics.density;
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
        getManager().startAccurateListening();
    }

    /* package */ DisplayAndroid(Display display) {
        mSdkDisplayId = display.getDisplayId();
        mObservers = new WeakHashMap<>();
        mSize = new Point();
        mPhysicalSize = new Point();
        mDisplayMetrics = new DisplayMetrics();
        updateFromDisplay(display);
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    /* package */ void updateFromDisplay(Display display) {
        display.getSize(mSize);
        display.getMetrics(mDisplayMetrics);

        if (hasForcedDIPScale()) mDisplayMetrics.density = sForcedDIPScale.floatValue();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            display.getRealSize(mPhysicalSize);
        }

        int newRotation = display.getRotation();
        boolean rotationChanged = newRotation != mRotation;
        mRotation = newRotation;

        if (rotationChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onRotationChanged(mRotation);
            }
        }
    }

    private DisplayAndroidObserver[] getObservers() {
        // Makes a copy to allow concurrent edit.
        return mObservers.keySet().toArray(EMPTY_OBSERVER_ARRAY);
    }
}
