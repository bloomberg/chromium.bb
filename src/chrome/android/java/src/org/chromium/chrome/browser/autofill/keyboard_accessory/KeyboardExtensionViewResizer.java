// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Px;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;

/**
 * This class is used by {@link ManualFillingMediator} to provide the combined height of
 * KeyboardAccessoryCoordinator and AccessorySheetCoordinator.
 */
class KeyboardExtensionViewResizer implements CompositorViewResizer {
    private int mHeight;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    @Override
    public @Px int getHeight() {
        return mHeight;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    void setKeyboardExtensionHeight(@Px int newKeyboardExtensionHeight) {
        if (mHeight == newKeyboardExtensionHeight) return;
        mHeight = newKeyboardExtensionHeight;
        for (Observer observer : mObservers) observer.onHeightChanged(mHeight);
    }
}