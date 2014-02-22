// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.ViewConfiguration;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * This class facilitates access to ViewConfiguration-related properties, also
 * providing native-code notifications when such properties have changed.
 *
 */
@JNINamespace("gfx")
public class ViewConfigurationHelper {
    private final Context mAppContext;
    private ViewConfiguration mViewConfiguration;

    private ViewConfigurationHelper(Context context) {
        mAppContext = context.getApplicationContext();
        mViewConfiguration = ViewConfiguration.get(mAppContext);
    }

    private void registerListener() {
        mAppContext.registerComponentCallbacks(
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration configuration) {
                        updateNativeViewConfigurationIfNecessary();
                    }

                    @Override
                    public void onLowMemory() {
                    }
                });
    }

    private void updateNativeViewConfigurationIfNecessary() {
        // The ViewConfiguration will differ only if the density has changed.
        ViewConfiguration configuration = ViewConfiguration.get(mAppContext);
        if (mViewConfiguration == configuration) return;

        mViewConfiguration = configuration;
        nativeUpdateSharedViewConfiguration(
                getScaledMaximumFlingVelocity(),
                getScaledMinimumFlingVelocity(),
                getScaledTouchSlop(),
                getScaledDoubleTapSlop());
    }

    @CalledByNative
    private static int getDoubleTapTimeout() {
        return ViewConfiguration.getDoubleTapTimeout();
    }

    @CalledByNative
    private static int getLongPressTimeout() {
        return ViewConfiguration.getLongPressTimeout();
    }

    @CalledByNative
    private static int getTapTimeout() {
        return ViewConfiguration.getTapTimeout();
    }

    @CalledByNative
    private static float getScrollFriction() {
        return ViewConfiguration.getScrollFriction();
    }

    @CalledByNative
    private int getScaledMaximumFlingVelocity() {
        return mViewConfiguration.getScaledMaximumFlingVelocity();
    }

    @CalledByNative
    private int getScaledMinimumFlingVelocity() {
        return mViewConfiguration.getScaledMinimumFlingVelocity();
    }

    @CalledByNative
    private int getScaledTouchSlop() {
        return mViewConfiguration.getScaledTouchSlop();
    }

    @CalledByNative
    private int getScaledDoubleTapSlop() {
        return mViewConfiguration.getScaledDoubleTapSlop();
    }

    @CalledByNative
    private static ViewConfigurationHelper createWithListener(Context context) {
        ViewConfigurationHelper viewConfigurationHelper = new ViewConfigurationHelper(context);
        viewConfigurationHelper.registerListener();
        return viewConfigurationHelper;
    }

    private native void nativeUpdateSharedViewConfiguration(
            int scaledMaximumFlingVelocity, int scaledMinimumFlingVelocity,
            int scaledTouchSlop, int scaledDoubleTapSlop);
}
