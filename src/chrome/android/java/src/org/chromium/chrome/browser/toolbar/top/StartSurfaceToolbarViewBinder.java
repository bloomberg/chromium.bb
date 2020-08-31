// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.APP_MENU_BUTTON_HELPER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IN_START_SURFACE_MODE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

// The view binder of the tasks surface view.
class StartSurfaceToolbarViewBinder {
    public static void bind(
            PropertyModel model, StartSurfaceToolbarView view, PropertyKey propertyKey) {
        if (propertyKey == ACCESSIBILITY_ENABLED) {
            view.onAccessibilityStatusChanged(model.get(ACCESSIBILITY_ENABLED));
        } else if (propertyKey == APP_MENU_BUTTON_HELPER) {
            view.setAppMenuButtonHelper(model.get(APP_MENU_BUTTON_HELPER));
        } else if (propertyKey == BUTTONS_CLICKABLE) {
            view.setButtonClickableState(model.get(BUTTONS_CLICKABLE));
        } else if (propertyKey == INCOGNITO_SWITCHER_VISIBLE) {
            view.setIncognitoSwitcherVisibility((Boolean) model.get(INCOGNITO_SWITCHER_VISIBLE));
        } else if (propertyKey == IDENTITY_DISC_AT_START) {
            view.setIdentityDiscAtStart(model.get(IDENTITY_DISC_AT_START));
        } else if (propertyKey == IDENTITY_DISC_CLICK_HANDLER) {
            view.setIdentityDiscClickHandler(model.get(IDENTITY_DISC_CLICK_HANDLER));
        } else if (propertyKey == IDENTITY_DISC_DESCRIPTION) {
            view.setIdentityDiscContentDescription(model.get(IDENTITY_DISC_DESCRIPTION));
        } else if (propertyKey == IDENTITY_DISC_IMAGE) {
            view.setIdentityDiscImage(model.get(IDENTITY_DISC_IMAGE));
        } else if (propertyKey == IDENTITY_DISC_IS_VISIBLE) {
            view.setIdentityDiscVisibility(model.get(IDENTITY_DISC_IS_VISIBLE));
        } else if (propertyKey == INCOGNITO_STATE_PROVIDER) {
            view.setIncognitoStateProvider(model.get(INCOGNITO_STATE_PROVIDER));
        } else if (propertyKey == IN_START_SURFACE_MODE) {
            view.setStartSurfaceMode(model.get(IN_START_SURFACE_MODE));
        } else if (propertyKey == IS_INCOGNITO) {
            view.updateIncognito(model.get(IS_INCOGNITO));
        } else if (propertyKey == IS_VISIBLE) {
            view.setToolbarVisibility(model.get(IS_VISIBLE));
        } else if (propertyKey == LOGO_IS_VISIBLE) {
            view.setLogoVisibility(model.get(LOGO_IS_VISIBLE));
        } else if (propertyKey == MENU_IS_VISIBLE) {
            view.setMenuButtonVisibility(model.get(MENU_IS_VISIBLE));
        } else if (propertyKey == NEW_TAB_CLICK_HANDLER) {
            view.setOnNewTabClickHandler(model.get(NEW_TAB_CLICK_HANDLER));
        } else if (propertyKey == NEW_TAB_BUTTON_AT_START) {
            view.setNewTabButtonAtStart(model.get(NEW_TAB_BUTTON_AT_START));
        } else if (propertyKey == NEW_TAB_BUTTON_IS_VISIBLE) {
            view.setNewTabButtonVisibility(model.get(NEW_TAB_BUTTON_IS_VISIBLE));
        }
    }
}
