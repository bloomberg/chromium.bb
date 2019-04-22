// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.ui.modelutil.PropertyModel;

class TouchlessNewTabPageMediator extends EmptyTabObserver {
    private static final String NAVIGATION_ENTRY_SCROLL_POSITION_KEY = "TouchlessScrollPosition";
    private static final String NAVIGATION_ENTRY_FOCUS_KEY = "TouchlessFocus";

    private final PropertyModel mModel;
    private final Tab mTab;
    private long mLastShownTimeNs;

    private ScrollPositionInfo mScrollPosition;
    private TouchlessNewTabPageFocusInfo mFocus;

    public TouchlessNewTabPageMediator(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);

        ScrollPositionInfo initialScrollPosition = ScrollPositionInfo.deserialize(
                NewTabPage.getStringFromNavigationEntry(tab, NAVIGATION_ENTRY_SCROLL_POSITION_KEY));
        TouchlessNewTabPageFocusInfo initialFocus = TouchlessNewTabPageFocusInfo.deserialize(
                NewTabPage.getStringFromNavigationEntry(tab, NAVIGATION_ENTRY_FOCUS_KEY));

        mModel = new PropertyModel.Builder(TouchlessNewTabPageProperties.ALL_KEYS)
                         .with(TouchlessNewTabPageProperties.FOCUS_CHANGE_CALLBACK,
                                 (newFocus) -> mFocus = newFocus)
                         .with(TouchlessNewTabPageProperties.INITIAL_FOCUS, initialFocus)
                         .with(TouchlessNewTabPageProperties.INITIAL_SCROLL_POSITION,
                                 initialScrollPosition)
                         .with(TouchlessNewTabPageProperties.SCROLL_POSITION_CALLBACK,
                                 (newScrollPosition) -> mScrollPosition = newScrollPosition)
                         .build();
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        // TODO(dewittj): Track loading status of the tiles before we record
        // shown and hidden states.
        recordNTPShown();
    }

    @Override
    public void onHidden(Tab tab, @Tab.TabHidingType int type) {
        recordNTPHidden();
    }

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        if (mScrollPosition != null) {
            NewTabPage.saveStringToNavigationEntry(
                    tab, NAVIGATION_ENTRY_SCROLL_POSITION_KEY, mScrollPosition.serialize());
        }
        if (mFocus != null) {
            NewTabPage.saveStringToNavigationEntry(
                    tab, NAVIGATION_ENTRY_FOCUS_KEY, mFocus.serialize());
        }
    }

    public PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        if (!mTab.isHidden()) recordNTPHidden();

        mTab.removeObserver(this);
    }

    /**
     * Records UMA for the NTP being shown. This includes a fresh page load or being brought to
     * the foreground.
     */
    private void recordNTPShown() {
        mLastShownTimeNs = System.nanoTime();
        NewTabPageUma.recordNTPImpression(NewTabPageUma.NTP_IMPRESSION_REGULAR);
        RecordUserAction.record("MobileNTPShown");
        SuggestionsMetrics.recordSurfaceVisible();
    }

    /** Records UMA for the NTP being hidden and the time spent on it. */
    private void recordNTPHidden() {
        NewTabPageUma.recordTimeSpentOnNtp(mLastShownTimeNs);
        SuggestionsMetrics.recordSurfaceHidden();
    }
}
