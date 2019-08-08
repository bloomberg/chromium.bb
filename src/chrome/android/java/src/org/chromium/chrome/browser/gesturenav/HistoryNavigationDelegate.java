// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Provides navigation-related configuration for pages using {@link HistoryNavigationLayout}.
 */
public abstract class HistoryNavigationDelegate {
    private final boolean mIsEnabled;
    private final boolean mDelegateSwipes;

    private HistoryNavigationDelegate(Context context) {
        mIsEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION)
                && (context instanceof ChromeActivity);
        mDelegateSwipes = ChromeFeatureList.isEnabled(ChromeFeatureList.DELEGATE_OVERSCROLL_SWIPES);
    }

    /**
     * @return {@code true} if history navigation is enabled.
     */
    public boolean isEnabled() {
        return mIsEnabled;
    }

    /**
     * @return {@code true} if swipe events are delegated to websites first.
     */
    public boolean delegateSwipes() {
        return mDelegateSwipes;
    }

    /**
     * @return {@link NavigationHandler#ActionDelegate} object.
     */
    public abstract NavigationHandler.ActionDelegate createActionDelegate();

    // Implementation for native pages with TabbedActionDelegate that uses Tab.goForward/goBack
    // to implement history navigation.
    private static class NativePageDelegate extends HistoryNavigationDelegate {
        private final Tab mTab;

        private NativePageDelegate(Tab tab) {
            super(tab.getActivity());
            mTab = tab;
        }

        @Override
        public NavigationHandler.ActionDelegate createActionDelegate() {
            return new TabbedActionDelegate(mTab);
        }
    }

    /**
     * Creates {@link HistoryNavigationDelegate} for a native page.
     */
    public static HistoryNavigationDelegate createForNativePage(Tab tab) {
        return new NativePageDelegate(tab);
    }

    // Implementation for tab switcher. Can't go forward, and going back exits
    // the switcher. Can exit Chrome if there's no current tab to go back to.
    private static class TabSwitcherNavigationDelegate extends HistoryNavigationDelegate {
        private final Runnable mBackPress;
        private final Supplier<Tab> mCurrentTab;

        private TabSwitcherNavigationDelegate(
                Context context, Runnable backPress, Supplier<Tab> currentTab) {
            super(context);
            mBackPress = backPress;
            mCurrentTab = currentTab;
        }

        @Override
        public NavigationHandler.ActionDelegate createActionDelegate() {
            return new TabSwitcherActionDelegate(mBackPress, mCurrentTab);
        }
    }

    /**
     * Creates {@link HistoryNavigationDelegate} for tab switcher.
     */
    public static HistoryNavigationDelegate createForTabSwitcher(
            Context context, Runnable backPress, Supplier<Tab> currentTab) {
        return new TabSwitcherNavigationDelegate(context, backPress, currentTab);
    }
}
