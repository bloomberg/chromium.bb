// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.VISIBLE;

import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;

/**
 * Observes {@link AccessorySheetProperties} changes (like a newly available tab) and triggers the
 * {@link AccessorySheetViewBinder} which will modify the view accordingly.
 */
class AccessorySheetViewBinder {
    public static void bind(PropertyModel model, AccessorySheetView view, PropertyKey propertyKey) {
        if (propertyKey == TABS) {
            view.setAdapter(AccessorySheetCoordinator.createTabViewAdapter(
                    model.get(TABS), view.getViewPager()));
        } else if (propertyKey == VISIBLE) {
            view.bringToFront(); // Ensure toolbars and other containers are overlaid.
            view.setVisibility(model.get(VISIBLE) ? View.VISIBLE : View.GONE);
            if (model.get(VISIBLE) && model.get(ACTIVE_TAB_INDEX) != NO_ACTIVE_TAB) {
                announceOpenedTab(view, model.get(TABS).get(model.get(ACTIVE_TAB_INDEX)));
            }
        } else if (propertyKey == HEIGHT) {
            ViewGroup.LayoutParams p = view.getLayoutParams();
            p.height = model.get(HEIGHT);
            view.setLayoutParams(p);
        } else if (propertyKey == TOP_SHADOW_VISIBLE) {
            view.setTopShadowVisible(model.get(TOP_SHADOW_VISIBLE));
        } else if (propertyKey == ACTIVE_TAB_INDEX) {
            if (model.get(ACTIVE_TAB_INDEX) != NO_ACTIVE_TAB) {
                view.setCurrentItem(model.get(ACTIVE_TAB_INDEX));
            }
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
        // Layout requests happen automatically since Kitkat and redundant requests cause warnings.
        if (view != null && Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            view.post(() -> {
                ViewParent parent = view.getParent();
                if (parent != null) {
                    parent.requestLayout();
                }
            });
        }
    }

    static void announceOpenedTab(View announcer, KeyboardAccessoryData.Tab tab) {
        if (tab.getOpeningAnnouncement() == null) return;
        announcer.announceForAccessibility(tab.getOpeningAnnouncement());
    }
}
