// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.lifecycle.ActivityResultWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;

/**
 * Dispatches lifecycle events of activities extending {@link AsyncInitializationActivity} to
 * registered observers.
 */
public class ActivityLifecycleDispatcher {
    private final ObserverList<InflationObserver> mInflationObservers = new ObserverList<>();
    private final ObserverList<NativeInitObserver> mNativeInitObservers = new ObserverList<>();
    private final ObserverList<PauseResumeWithNativeObserver> mPauseResumeObservers =
            new ObserverList<>();
    private final ObserverList<StartStopWithNativeObserver> mStartStopObservers =
            new ObserverList<>();
    private final ObserverList<Destroyable> mDestroyables = new ObserverList<>();
    private final ObserverList<SaveInstanceStateObserver> mSaveInstanceStateObservers =
            new ObserverList<>();
    private final ObserverList<WindowFocusChangedObserver> mWindowFocusChangesObservers =
            new ObserverList<>();
    private final ObserverList<ActivityResultWithNativeObserver>
            mActivityResultWithNativeObservers = new ObserverList<>();

    /**
     * Registers an observer.
     * @param observer must implement one or several observer interfaces in
     * {@link org.chromium.chrome.browser.lifecycle} in order to receive corresponding events.
     */
    public void register(LifecycleObserver observer) {
        if (observer instanceof InflationObserver) {
            mInflationObservers.addObserver((InflationObserver) observer);
        }
        if (observer instanceof PauseResumeWithNativeObserver) {
            mPauseResumeObservers.addObserver((PauseResumeWithNativeObserver) observer);
        }
        if (observer instanceof StartStopWithNativeObserver) {
            mStartStopObservers.addObserver((StartStopWithNativeObserver) observer);
        }
        if (observer instanceof NativeInitObserver) {
            mNativeInitObservers.addObserver((NativeInitObserver) observer);
        }
        if (observer instanceof Destroyable) {
            mDestroyables.addObserver((Destroyable) observer);
        }
        if (observer instanceof SaveInstanceStateObserver) {
            mSaveInstanceStateObservers.addObserver((SaveInstanceStateObserver) observer);
        }
        if (observer instanceof WindowFocusChangedObserver) {
            mWindowFocusChangesObservers.addObserver((WindowFocusChangedObserver) observer);
        }
        if (observer instanceof ActivityResultWithNativeObserver) {
            mActivityResultWithNativeObservers.addObserver(
                    (ActivityResultWithNativeObserver) observer);
        }
    }

    /**
     * Unregisters an observer.
     */
    public void unregister(LifecycleObserver observer) {
        if (observer instanceof InflationObserver) {
            mInflationObservers.removeObserver((InflationObserver) observer);
        }
        if (observer instanceof PauseResumeWithNativeObserver) {
            mPauseResumeObservers.removeObserver((PauseResumeWithNativeObserver) observer);
        }
        if (observer instanceof StartStopWithNativeObserver) {
            mStartStopObservers.removeObserver((StartStopWithNativeObserver) observer);
        }
        if (observer instanceof NativeInitObserver) {
            mNativeInitObservers.removeObserver((NativeInitObserver) observer);
        }
        if (observer instanceof Destroyable) {
            mDestroyables.removeObserver((Destroyable) observer);
        }
        if (observer instanceof SaveInstanceStateObserver) {
            mSaveInstanceStateObservers.removeObserver((SaveInstanceStateObserver) observer);
        }
        if (observer instanceof WindowFocusChangedObserver) {
            mWindowFocusChangesObservers.removeObserver((WindowFocusChangedObserver) observer);
        }
        if (observer instanceof ActivityResultWithNativeObserver) {
            mActivityResultWithNativeObservers.removeObserver(
                    (ActivityResultWithNativeObserver) observer);
        }
    }

    void dispatchPreInflationStartup() {
        for (InflationObserver observer : mInflationObservers) {
            observer.onPreInflationStartup();
        }
    }

    void dispatchPostInflationStartup() {
        for (InflationObserver observer : mInflationObservers) {
            observer.onPostInflationStartup();
        }
    }

    void dispatchOnResumeWithNative() {
        for (PauseResumeWithNativeObserver observer : mPauseResumeObservers) {
            observer.onResumeWithNative();
        }
    }

    void dispatchOnPauseWithNative() {
        for (PauseResumeWithNativeObserver observer : mPauseResumeObservers) {
            observer.onPauseWithNative();
        }
    }

    void dispatchOnStartWithNative() {
        for (StartStopWithNativeObserver observer : mStartStopObservers) {
            observer.onStartWithNative();
        }
    }

    void dispatchOnStopWithNative() {
        for (StartStopWithNativeObserver observer : mStartStopObservers) {
            observer.onStopWithNative();
        }
    }

    void dispatchNativeInitializationFinished() {
        for (NativeInitObserver observer : mNativeInitObservers) {
            observer.onFinishNativeInitialization();
        }
    }

    void dispatchOnDestroy() {
        for (Destroyable destroyable : mDestroyables) {
            destroyable.destroy();
        }
    }

    void dispatchOnSaveInstanceState(Bundle outBundle) {
        for (SaveInstanceStateObserver observer: mSaveInstanceStateObservers) {
            observer.onSaveInstanceState(outBundle);
        }
    }

    void dispatchOnWindowFocusChanged(boolean hasFocus) {
        for (WindowFocusChangedObserver observer: mWindowFocusChangesObservers) {
            observer.onWindowFocusChanged(hasFocus);
        }
    }

    void dispatchOnActivityResultWithNative(int requestCode, int resultCode, Intent data) {
        for (ActivityResultWithNativeObserver observer : mActivityResultWithNativeObservers) {
            observer.onActivityResultWithNative(requestCode, resultCode, data);
        }
    }
}
