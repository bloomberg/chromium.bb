// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_activity_glue;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.night_mode.NightModeReparentingController;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;

/** Constructs delegates needed for reparenting tabs. */
public class ReparentingDelegateFactory {
    /**
     * @return Creates an implementation of {@link ReparentingTask.Delegate} that supplies
     *         dependencies for {@link ReparentingTask} to reparent a Tab.
     */
    public static ReparentingTask.Delegate createReparentingTaskDelegate(
            final CompositorViewHolder compositorViewHolder, final WindowAndroid windowAndroid,
            TabDelegateFactory tabDelegateFactory) {
        return new ReparentingTask.Delegate() {
            @Override
            public CompositorViewHolder getCompositorViewHolder() {
                return compositorViewHolder;
            }

            @Override
            public WindowAndroid getWindowAndroid() {
                return windowAndroid;
            }

            @Override
            public TabDelegateFactory getTabDelegateFactory() {
                return tabDelegateFactory;
            }
        };
    }

    /**
     * @return Creates an implementation of {@link NightModeReparentingController.Delegate} that
     *         supplies dependencies to {@link NightModeReparentingController}.
     */
    public static NightModeReparentingController.Delegate
    createNightModeReparentingControllerDelegate(final ActivityTabProvider activityTabProvider,
            final TabModelSelector tabModelSelector) {
        return new NightModeReparentingController.Delegate() {
            @Override
            public ActivityTabProvider getActivityTabProvider() {
                return activityTabProvider;
            }

            @Override
            public TabModelSelector getTabModelSelector() {
                return tabModelSelector;
            }

            @Override
            public boolean isNTPUrl(String url) {
                return NewTabPage.isNTPUrl(url);
            }
        };
    }
}
