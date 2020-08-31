// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.os.IBinder;
import android.provider.Settings;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.lang.reflect.Method;
import java.util.Arrays;

/**
 * {@link TestRule} to disable animations for UI testing, or enable animation with new
 * DisableAnimationsTestRule(true).
 */
public class DisableAnimationsTestRule implements TestRule {
    private boolean mEnableAnimation;
    private Method mSetAnimationScalesMethod;
    private Method mGetAnimationScalesMethod;
    private Object mWindowManagerObject;

    private static final float DISABLED_SCALE_FACTOR = 0.0f;
    private static final float DEFAULT_SCALE_FACTOR = 1.0f;
    private static final String TAG = "DisableAnimations";

    // Disable animations by default.
    public DisableAnimationsTestRule() {
        this(false);
    }

    /**
     * Invoke setAnimationScalesMethod to turn off system animations, such as Window animation
     * scale, Transition animation scale, Animator duration scale, which can improve stability
     * and reduce flakiness for UI testing.
     */
    public DisableAnimationsTestRule(boolean enableAnimation) {
        mEnableAnimation = enableAnimation;
        try {
            Class<?> windowManagerStubClazz = Class.forName("android.view.IWindowManager$Stub");
            Method asInterface =
                    windowManagerStubClazz.getDeclaredMethod("asInterface", IBinder.class);
            Class<?> serviceManagerClazz = Class.forName("android.os.ServiceManager");
            Method getService = serviceManagerClazz.getDeclaredMethod("getService", String.class);
            Class<?> windowManagerClazz = Class.forName("android.view.IWindowManager");
            mSetAnimationScalesMethod =
                    windowManagerClazz.getDeclaredMethod("setAnimationScales", float[].class);
            mGetAnimationScalesMethod = windowManagerClazz.getDeclaredMethod("getAnimationScales");
            IBinder windowManagerBinder = (IBinder) getService.invoke(null, "window");
            mWindowManagerObject = asInterface.invoke(null, windowManagerBinder);
        } catch (Exception e) {
            throw new RuntimeException("Failed to access animation methods", e);
        }
    }

    @Override
    public Statement apply(final Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                float curAnimationScale = Settings.Global.getFloat(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, DEFAULT_SCALE_FACTOR);
                float toAnimationScale =
                        mEnableAnimation ? DEFAULT_SCALE_FACTOR : DISABLED_SCALE_FACTOR;
                if (curAnimationScale != toAnimationScale) {
                    setAnimationScaleFactors(toAnimationScale);
                    Log.i(TAG, "Set animation scales to: %.1f", toAnimationScale);
                }
                try {
                    statement.evaluate();
                } finally {
                    if (curAnimationScale != toAnimationScale) {
                        setAnimationScaleFactors(curAnimationScale);
                        Log.i(TAG, "Set animation scales to: %.1f", curAnimationScale);
                    }
                }
            }
        };
    }

    private void setAnimationScaleFactors(float scaleFactor) throws Exception {
        float[] scaleFactors = (float[]) mGetAnimationScalesMethod.invoke(mWindowManagerObject);
        Arrays.fill(scaleFactors, scaleFactor);
        mSetAnimationScalesMethod.invoke(mWindowManagerObject, scaleFactors);
    }
}
