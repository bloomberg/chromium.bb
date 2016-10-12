// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.annotation.SuppressLint;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build;
import android.util.SparseArray;
import android.view.Display;
import android.view.WindowManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.ui.gfx.DeviceDisplayInfo;

/**
 * DisplayAndroidManager is a class that informs its observers Display changes.
 */
/* package */ class DisplayAndroidManager {
    /**
     * DisplayListenerBackend is an interface that abstract the mechanism used for the actual
     * display update listening. The reason being that from Android API Level 17 DisplayListener
     * will be used. Before that, an unreliable solution based on onConfigurationChanged has to be
     * used.
     */
    private interface DisplayListenerBackend {

        /**
         * Starts to listen for display changes. This will be called
         * when the first observer is added.
         */
        void startListening();

        /**
         * Toggle the accurate mode if it wasn't already doing so. The backend
         * will keep track of the number of times this has been called.
         */
        void startAccurateListening();

        /**
         * Request to stop the accurate mode. It will effectively be stopped
         * only if this method is called as many times as
         * startAccurateListening().
         */
        void stopAccurateListening();
    }

    /**
     * DisplayListenerAPI16 implements DisplayListenerBackend
     * to use ComponentCallbacks in order to listen for display
     * changes.
     *
     * This method is known to not correctly detect 180 degrees changes but it
     * is the only method that will work before API Level 17 (excluding polling).
     * When toggleAccurateMode() is called, it will start polling in order to
     * find out if the display has changed.
     */
    private class DisplayListenerAPI16
            implements DisplayListenerBackend, ComponentCallbacks {

        private static final long POLLING_DELAY = 500;

        private int mMainSdkDisplayId;
        private int mAccurateCount;

        public DisplayListenerAPI16(int sdkDisplayId) {
            mMainSdkDisplayId = sdkDisplayId;
        }

        // DisplayListenerBackend implementation:

        @Override
        public void startListening() {
            getContext().registerComponentCallbacks(this);
        }

        @Override
        public void startAccurateListening() {
            ++mAccurateCount;

            if (mAccurateCount > 1) return;

            // Start polling if we went from 0 to 1. The polling will
            // automatically stop when mAccurateCount reaches 0.
            final DisplayListenerAPI16 self = this;
            ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    self.onConfigurationChanged(null);

                    if (self.mAccurateCount < 1) return;

                    ThreadUtils.postOnUiThreadDelayed(this,
                            DisplayListenerAPI16.POLLING_DELAY);
                }
            }, POLLING_DELAY);
        }

        @Override
        public void stopAccurateListening() {
            --mAccurateCount;
            assert mAccurateCount >= 0;
        }

        // ComponentCallbacks implementation:

        @Override
        public void onConfigurationChanged(Configuration newConfig) {
            updateDeviceDisplayInfo();
            mIdMap.get(mMainSdkDisplayId).updateFromDisplay(getDisplayFromContext(getContext()));
        }

        @Override
        public void onLowMemory() {
        }
    }

    /**
     * DisplayListenerBackendImpl implements DisplayListenerBackend
     * to use DisplayListener in order to listen for display changes.
     *
     * This method is reliable but DisplayListener is only available for API Level 17+.
     */
    @SuppressLint("NewApi")
    private class DisplayListenerBackendImpl
            implements DisplayListenerBackend, DisplayListener {

        // DisplayListenerBackend implementation:

        @Override
        public void startListening() {
            getDisplayManager().registerDisplayListener(this, null);
        }

        @Override
        public void startAccurateListening() {
            // Always accurate. Do nothing.
        }

        @Override
        public void stopAccurateListening() {
            // Always accurate. Do nothing.
        }

        // DisplayListener implementation:

        @Override
        public void onDisplayAdded(int sdkDisplayId) {
            // DisplayAndroid is added lazily on first use. This is to workaround corner case
            // bug where DisplayManager.getDisplay(sdkDisplayId) returning null here.
        }

        @Override
        public void onDisplayRemoved(int sdkDisplayId) {
            mIdMap.remove(sdkDisplayId);
        }

        @Override
        public void onDisplayChanged(int sdkDisplayId) {
            updateDeviceDisplayInfo();
            DisplayAndroid displayAndroid = mIdMap.get(sdkDisplayId);
            if (displayAndroid != null) {
                displayAndroid.updateFromDisplay(getDisplayManager().getDisplay(sdkDisplayId));
            }
        }
    }

    private static DisplayAndroidManager sDisplayAndroidManager;

    private SparseArray<DisplayAndroid> mIdMap;
    private final DisplayListenerBackend mBackend;

    /* package */ static DisplayAndroidManager getInstance() {
        if (sDisplayAndroidManager == null) {
            sDisplayAndroidManager = new DisplayAndroidManager();
        }
        return sDisplayAndroidManager;
    }

    /* package */ static Display getDisplayFromContext(Context context) {
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        return windowManager.getDefaultDisplay();
    }

    private static Context getContext() {
        // Using the global application context is probably ok.
        // The DisplayManager API observers all display updates, so in theory it should not matter
        // which context is used to obtain it. If this turns out not to be true in practice, it's
        // possible register from all context used though quite complex.
        return ContextUtils.getApplicationContext();
    }

    private static DisplayManager getDisplayManager() {
        return (DisplayManager) getContext().getSystemService(Context.DISPLAY_SERVICE);
    }

    private static void updateDeviceDisplayInfo() {
        DeviceDisplayInfo.create(getContext()).updateNativeSharedDisplayInfo();
    }

    private DisplayAndroidManager() {
        mIdMap = new SparseArray<>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            mBackend = new DisplayListenerBackendImpl();
        } else {
            Display display = getDisplayFromContext(getContext());
            mBackend = new DisplayListenerAPI16(display.getDisplayId());
            addDisplay(display);  // Note this display is never removed.
        }
        mBackend.startListening();
    }

    /* package */ DisplayAndroid getDisplayAndroid(Display display) {
        int sdkDisplayId = display.getDisplayId();
        DisplayAndroid displayAndroid = mIdMap.get(sdkDisplayId);
        if (displayAndroid == null) {
            displayAndroid = addDisplay(display);
        }
        return displayAndroid;
    }

    /* package */ void startAccurateListening() {
        mBackend.startAccurateListening();
    }

    /* package */ void stopAccurateListening() {
        mBackend.stopAccurateListening();
    }

    private DisplayAndroid addDisplay(Display display) {
        int sdkDisplayId = display.getDisplayId();
        DisplayAndroid displayAndroid = new DisplayAndroid(display);
        mIdMap.put(sdkDisplayId, displayAndroid);
        return displayAndroid;
    }
}
