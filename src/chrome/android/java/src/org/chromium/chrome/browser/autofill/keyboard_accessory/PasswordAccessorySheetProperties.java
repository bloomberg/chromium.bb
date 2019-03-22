// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * This class holds properties that are used to build a model for the password accessory sheet
 * component. These properties store the state of the {@link PasswordAccessorySheetView} which is
 * bound to model using the {@link PasswordAccessorySheetViewBinder}.
 * It is built in the {@link PasswordAccessorySheetCoordinator} and modified by the
 * {@link PasswordAccessorySheetMediator}.
 */
class PasswordAccessorySheetProperties {
    static final ReadableObjectPropertyKey<ListModel<KeyboardAccessoryData.Item>> CREDENTIALS =
            new ReadableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<RecyclerView.OnScrollListener> SCROLL_LISTENER =
            new WritableObjectPropertyKey<>();

    private PasswordAccessorySheetProperties() {}
}
