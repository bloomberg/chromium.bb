// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.View;

/**
 * From the Chromium architecture point of view, ViewAndroid and its native counterpart
 * serve purpose of representing Android view where Chrome expects to have a cross platform
 * handle to the system view type. As Views are Java object on Android, this ViewAndroid
 * and its native counterpart provide the expected abstractions on the C++ side and allow
 * it to be flexibly glued to an actual Android Java View at runtime.
 *
 * It should only be used where access to Android Views is needed from the C++ code.
 */
public class ViewAndroid {
    // Native pointer to the c++ ViewAndroid object.
    private final ViewAndroidDelegate mViewAndroidDelegate;
    private int mKeepScreenOnCount;
    private View mKeepScreenOnView;

    /**
     * Constructs a View object.
     */
    public ViewAndroid(ViewAndroidDelegate viewAndroidDelegate) {
        mViewAndroidDelegate = viewAndroidDelegate;
    }

    public ViewAndroidDelegate getViewAndroidDelegate() {
        return mViewAndroidDelegate;
    }

    /**
     * Set KeepScreenOn flag. If the flag already set, increase mKeepScreenOnCount.
     */
    public void incrementKeepScreenOnCount() {
        mKeepScreenOnCount++;
        if (mKeepScreenOnCount == 1) {
            mKeepScreenOnView = mViewAndroidDelegate.acquireAnchorView();
            mViewAndroidDelegate.setAnchorViewPosition(mKeepScreenOnView, 0, 0, 0, 0);
            mKeepScreenOnView.setKeepScreenOn(true);
        }
    }

    /**
     * Decrease mKeepScreenOnCount, if it is decreased to 0, remove the flag.
     */
    public void decrementKeepScreenOnCount() {
        assert mKeepScreenOnCount > 0;
        mKeepScreenOnCount--;
        if (mKeepScreenOnCount == 0) {
            mViewAndroidDelegate.releaseAnchorView(mKeepScreenOnView);
            mKeepScreenOnView = null;
        }
    }
}
