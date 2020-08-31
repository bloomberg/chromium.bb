// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.app.Activity;

import org.chromium.chrome.browser.native_page.NativePageNavigationDelegateImpl;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * Extension of {@link NativePageNavigationDelegate} with suggestions-specific methods.
 */
public class SuggestionsNavigationDelegate extends NativePageNavigationDelegateImpl {
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";

    public SuggestionsNavigationDelegate(Activity activity, Profile profile, NativePageHost host,
            TabModelSelector tabModelSelector, Tab tab) {
        super(activity, profile, host, tabModelSelector, tab);
    }

    @Override
    public void navigateToHelpPage() {
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_LEARN_MORE);
        // TODO(dgn): Use the standard Help UI rather than a random link to online help?
        openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(NEW_TAB_URL_HELP, PageTransition.AUTO_BOOKMARK));
    }

    /**
     * Opens the suggestions page without recording metrics.
     *
     * @param windowOpenDisposition How to open (new window, current tab, etc).
     * @param url The url to navigate to.
     */
    public void navigateToSuggestionUrl(int windowOpenDisposition, String url) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        openUrl(windowOpenDisposition, loadUrlParams);
    }
}
