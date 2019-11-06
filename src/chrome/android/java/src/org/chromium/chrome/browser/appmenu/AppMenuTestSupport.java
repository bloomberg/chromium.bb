// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;

/**
 * Utility methods for performing operations on the app menu needed for testing.
 *
 * TODO(https://crbug.com/956260): This will live in a support/ package once app menu code
 * is migrated to have its own build target. For now it lives here so this class may access package
 * protected appmenu classes while still allowing classes in chrome_java_test_support may access
 * AppMenuTestSupport.
 */
public class AppMenuTestSupport {
    /**
     * @param The {@link ChromeActivity} for the current test.
     * @return The {@link Menu} held by the app menu.
     */
    public static Menu getMenu(ChromeActivity activity) {
        return getAppMenuCoordinator(activity)
                .getAppMenuHandlerImplForTesting()
                .getAppMenu()
                .getMenu();
    }

    /**
     * See {@link AppMenuHandlerImpl#onOptionsItemSelected(MenuItem)}.
     */
    public static void onOptionsItemSelected(ChromeActivity activity, MenuItem item) {
        getAppMenuCoordinator(activity).getAppMenuHandlerImplForTesting().onOptionsItemSelected(
                item);
    }

    /**
     * See {@link AppMenuHandlerImpl#showAppMenu(View, boolean, boolean)}.
     */
    public static boolean showAppMenu(ChromeActivity activity, View anchorView,
            boolean startDragging, boolean showFromBottom) {
        return getAppMenuCoordinator(activity).getAppMenuHandlerImplForTesting().showAppMenu(
                anchorView, startDragging, showFromBottom);
    }

    private static AppMenuCoordinatorImpl getAppMenuCoordinator(ChromeActivity activity) {
        return ((AppMenuCoordinatorImpl) activity.getRootUiCoordinatorForTesting()
                        .getAppMenuCoordinatorForTesting());
    }
}
