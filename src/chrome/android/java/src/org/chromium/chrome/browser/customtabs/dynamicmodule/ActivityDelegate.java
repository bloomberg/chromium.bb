// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.Bundle;
import android.os.RemoteException;

import org.chromium.base.VisibleForTesting;

/**
 * A wrapper around a {@link IActivityDelegate}.
 *
 * No {@link RemoteException} should ever be thrown as all of this code runs in the same process.
 */
public class ActivityDelegate {
    private final IActivityDelegate mActivityDelegate;

    private interface RemoteRunnable { void run() throws RemoteException; }

    private interface RemoteCallable<T> { T call() throws RemoteException; }

    private void safeRun(RemoteRunnable runnable) {
        try {
            runnable.run();
        } catch (RemoteException e) {
            assert false;
        }
    }

    private <T> T safeCall(RemoteCallable<T> callable, T defaultReturn) {
        try {
            return callable.call();
        } catch (RemoteException e) {
            assert false;
        }

        return defaultReturn;
    }

    public ActivityDelegate(IActivityDelegate activityDelegate) {
        mActivityDelegate = activityDelegate;
    }

    public void onCreate(Bundle savedInstanceState) {
        safeRun(() -> mActivityDelegate.onCreate(savedInstanceState));
    }

    public void onPostCreate(Bundle savedInstanceState) {
        safeRun(() -> mActivityDelegate.onPostCreate(savedInstanceState));
    }

    public void onStart() {
        safeRun(mActivityDelegate::onStart);
    }

    public void onStop() {
        safeRun(mActivityDelegate::onStop);
    }

    public void onWindowFocusChanged(boolean hasFocus) {
        safeRun(() -> mActivityDelegate.onWindowFocusChanged(hasFocus));
    }

    public void onSaveInstanceState(Bundle outState) {
        safeRun(() -> mActivityDelegate.onSaveInstanceState(outState));
    }

    public void onRestoreInstanceState(Bundle savedInstanceState) {
        safeRun(() -> mActivityDelegate.onRestoreInstanceState(savedInstanceState));
    }

    public void onResume() {
        safeRun(mActivityDelegate::onResume);
    }

    public void onPause() {
        safeRun(mActivityDelegate::onPause);
    }

    public void onDestroy() {
        safeRun(mActivityDelegate::onDestroy);
    }

    public boolean onBackPressed() {
        return safeCall(mActivityDelegate::onBackPressed, false);
    }

    public void onBackPressedAsync(Runnable notHandledRunnable) {
        safeRun(() -> mActivityDelegate.onBackPressedAsync(ObjectWrapper.wrap(notHandledRunnable)));
    }

    public void onNavigationEvent(int navigationEvent, Bundle extras) {
        safeRun(() -> mActivityDelegate.onNavigationEvent(navigationEvent, extras));
    }

    public void onMessageChannelReady() {
        safeRun(mActivityDelegate::onMessageChannelReady);
    }

    public void onPostMessage(String message) {
        safeRun(() -> mActivityDelegate.onPostMessage(message));
    }

    @VisibleForTesting
    public IActivityDelegate getIActivityDelegateForTesting() {
        return mActivityDelegate;
    }
}
