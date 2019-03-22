// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.CREDENTIALS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.SCROLL_LISTENER;

import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * This class contains all logic for the password accessory sheet component. Changes to its internal
 * {@link PropertyModel} are observed by a {@link PropertyModelChangeProcessor} and affect the
 * {@link PasswordAccessorySheetView}.
 */
class PasswordAccessorySheetMediator implements KeyboardAccessoryData.Observer<Item[]> {
    private final PropertyModel mModel;

    @Override
    public void onItemAvailable(int typeId, Item[] items) {
        mModel.get(CREDENTIALS).set(items);
    }

    PasswordAccessorySheetMediator(
            PropertyModel model, @Nullable RecyclerView.OnScrollListener scrollListener) {
        mModel = model;
        mModel.set(SCROLL_LISTENER, scrollListener);
    }

    void onTabShown() {
        KeyboardAccessoryMetricsRecorder.recordActionImpression(AccessoryAction.MANAGE_PASSWORDS);
        KeyboardAccessoryMetricsRecorder.recordSheetSuggestions(
                AccessoryTabType.PASSWORDS, mModel.get(CREDENTIALS));
    }

    @VisibleForTesting
    ListModel<Item> getModelForTesting() {
        return mModel.get(CREDENTIALS);
    }
}
