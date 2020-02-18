// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * IdentityManager provides access to native IdentityManager's public API to java components.
 */
public class IdentityManager {
    private static final String TAG = "IdentityManager";

    /**
     * IdentityManager.Observer is notified when the available account information are updated. This
     * is a subset of native's IdentityManager::Observer.
     */
    public interface Observer {
        /**
         * Called when an account becomes the user's primary account.
         * This method is not called during a reauth.
         */
        void onPrimaryAccountSet(CoreAccountInfo account);

        /**
         * Called when the user moves from having a primary account to no longer having a primary
         * account (note that the user may still have an *unconsented* primary account after this
         * event).
         */
        void onPrimaryAccountCleared(CoreAccountInfo account);
    }

    private long mNativeIdentityManager;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Called by native to create an instance of IdentityManager.
     */
    @CalledByNative
    static private IdentityManager create(long nativeIdentityManager) {
        return new IdentityManager(nativeIdentityManager);
    }
    private IdentityManager(long nativeIdentityManager) {
        assert nativeIdentityManager != 0;

        mNativeIdentityManager = nativeIdentityManager;
    }

    /**
     * Called by native upon KeyedService's shutdown
     */
    @CalledByNative
    private void destroy() {
        mNativeIdentityManager = 0;
    }

    /**
     * Registers a IdentityManager.Observer
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Unregisters a IdentityManager.Observer
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies observers that the primary account was set in C++.
     */
    @CalledByNative
    private void onPrimaryAccountSet(CoreAccountInfo account) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountSet(account);
        }
    }

    /**
     * Notifies observers that the primary account was cleared in C++.
     */
    @CalledByNative
    private void onPrimaryAccountCleared(CoreAccountInfo account) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountCleared(account);
        }
    }

    /**
     * Returns whether the user's primary account is available.
     */
    boolean hasPrimaryAccount() {
        return IdentityManagerJni.get().hasPrimaryAccount(mNativeIdentityManager);
    }

    @NativeMethods
    interface Natives {
        public boolean hasPrimaryAccount(long nativeIdentityManager);
    }
}
