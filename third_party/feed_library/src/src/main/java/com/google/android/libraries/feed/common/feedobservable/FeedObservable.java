// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.feedobservable;

import com.google.android.libraries.feed.common.logging.Logger;

import java.util.HashSet;
import java.util.Set;

/** Provides methods for registering or unregistering arbitrary observers. */
public abstract class FeedObservable<ObserverT> implements Observable<ObserverT> {
    private static final String TAG = "FeedObservable";

    protected final Set<ObserverT> observers = new HashSet<>();

    /** Adds given {@code observer}. No-op if the observer has already been added. */
    @Override
    public void registerObserver(ObserverT observer) {
        synchronized (observers) {
            if (!observers.add(observer)) {
                Logger.w(TAG, "Registering observer: %s multiple times.", observer);
            }
        }
    }

    /** Removes given {@code observer}. No-op if the observer is not currently added. */
    @Override
    public void unregisterObserver(ObserverT observer) {
        synchronized (observers) {
            if (!observers.remove(observer)) {
                Logger.w(TAG, "Unregistering observer: %s that isn't registered.", observer);
            }
        }
    }
}
