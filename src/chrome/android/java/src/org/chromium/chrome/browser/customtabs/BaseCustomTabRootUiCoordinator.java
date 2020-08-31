// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.app.reengagement.ReengagementActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.reengagement.ReengagementNotificationController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.components.feature_engagement.Tracker;

/**
 * A {@link RootUiCoordinator} variant that controls UI for {@link BaseCustomTabActivity}.
 */
public class BaseCustomTabRootUiCoordinator
        extends RootUiCoordinator implements NativeInitObserver {
    private final CustomTabToolbarCoordinator mToolbarCoordinator;
    private final CustomTabActivityNavigationController mNavigationController;

    public BaseCustomTabRootUiCoordinator(ChromeActivity activity,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            CustomTabToolbarCoordinator customTabToolbarCoordinator,
            CustomTabActivityNavigationController customTabNavigationController,
            ActivityTabProvider tabProvider, ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier) {
        super(activity, null, shareDelegateSupplier, tabProvider, profileSupplier,
                bookmarkBridgeSupplier);

        mToolbarCoordinator = customTabToolbarCoordinator;
        mNavigationController = customTabNavigationController;
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();

        mToolbarCoordinator.onToolbarInitialized(mToolbarManager);
        mNavigationController.onToolbarInitialized(mToolbarManager);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (!ReengagementNotificationController.isEnabled()) return;
        new OneShotCallback<>(mProfileSupplier, mCallbackController.makeCancelable(profile -> {
            assert profile != null : "Unexpectedly null profile from TabModel.";
            if (profile == null) return;
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            ReengagementNotificationController controller = new ReengagementNotificationController(
                    mActivity, tracker, ReengagementActivity.class);
            controller.tryToReengageTheUser();
        }));
    }
}
