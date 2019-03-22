// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and triggers
 * the {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryModernViewBinder {
    static class ModernActionViewHolder extends KeyboardAccessoryViewBinder.ActionViewHolder {
        public ModernActionViewHolder(View actionView) {
            super(actionView);
        }

        public static KeyboardAccessoryViewBinder.ActionViewHolder create(
                ViewGroup parent, @AccessoryAction int viewType) {
            switch (viewType) {
                case AccessoryAction.AUTOFILL_SUGGESTION:
                    return new ModernActionViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(
                                            R.layout.keyboard_accessory_suggestion, parent, false));
                case AccessoryAction.TAB_SWITCHER:
                    return new ModernActionViewHolder(
                            LayoutInflater.from(parent.getContext())
                                    .inflate(R.layout.keyboard_accessory_tabs, parent, false));
            }
            return KeyboardAccessoryViewBinder.ActionViewHolder.create(parent, viewType);
        }

        @Override
        public void bind(Action action) {
            if (action.getActionType() == AccessoryAction.TAB_SWITCHER) return;
            super.bind(action);
        }
    }

    public static void bind(
            PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        assert view instanceof KeyboardAccessoryModernView;
        KeyboardAccessoryModernView modernView = (KeyboardAccessoryModernView) view;
        boolean wasBound = KeyboardAccessoryViewBinder.bindInternal(model, modernView, propertyKey);
        if (propertyKey == ACTIVE_TAB) {
            modernView.setKeyboardToggleVisibility(model.get(ACTIVE_TAB) != null);
        } else if (propertyKey == SHOW_KEYBOARD_CALLBACK) {
            modernView.setShowKeyboardCallback(model.get(SHOW_KEYBOARD_CALLBACK));
        } else {
            assert wasBound : "Every possible property update needs to be handled!";
        }
        KeyboardAccessoryViewBinder.requestLayoutPreKitkat(modernView);
    }
}
