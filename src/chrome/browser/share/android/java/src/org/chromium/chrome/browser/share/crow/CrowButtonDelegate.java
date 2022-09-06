// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Interface to expose the experimental 'Crow' sharing feature in the App menu
 * footer and Feed to external classes.
 */
public interface CrowButtonDelegate {
    /**
     * @return Whether to show the chip for |url|.
     *
     * @param url URL for current tab where the app menu was launched.
     */
    boolean isEnabledForSite(GURL url);

    /**
     * Launches a custom tab to a server-provided interaction flow.
     * Uses URL defined by the study config.
     *
     * @param currentContext the current Context for which the user activated an
     *                        entry point.
     * @param pageUrl URL for the page; passed in rather than derived from currentTab
     *     or WebContents's lastCommittedURL as it was used to construct UI in the caller.
     * @param canonicalUrl Canonical URL for 'pageUrl.' May be empty.
     * @param isFollowing Whether the user is following the associated host in the feed.
     */
    void launchCustomTab(
            Context currentContext, GURL pageUrl, GURL canonicalUrl, boolean isFollowing);

    /**
     * @return experiment-configured chip text.
     */
    String getButtonText();

    /**
     * Obtains the Canonical URL for a Tab.
     * @param tab The tab for which to find the canonical URL.
     * @param Callback<String> callback returning the canonical URL, or empty.
     */
    void requestCanonicalUrl(Tab tab, Callback<GURL> url);
}
