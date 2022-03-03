// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.LoadUrlParams;

/** Interface for Feed actions implemented by the Browser.*/
public interface FeedActionDelegate {
    /** Information about a page visit. */
    public class VisitResult {
        // Total page visit time.
        public long visitTimeMs;
    }
    /** Downloads a web page.*/
    void downloadPage(String url);

    /**
     * Opens a url that was presented to the user as suggested content.
     * @param disposition A `org.chromium.ui.mojom.WindowOpenDisposition` value.
     * @param params What to load.
     * @param onPageLoaded Called when the page completes loading.
     * @param onVisitComplete Called when the user closes or navigates away from the page.
     */
    void openSuggestionUrl(int disposition, LoadUrlParams params, Runnable onPageLoaded,
            Callback<VisitResult> onVisitComplete);

    /**
     * Opens a page.
     * @param disposition A `org.chromium.ui.mojom.WindowOpenDisposition` value.
     * @param params What to load.
     */
    void openUrl(int disposition, LoadUrlParams params);

    /**
     * Opens the NTP help page.
     */
    void openHelpPage();

    /**
     * Add an item to the reading list.
     */
    void addToReadingList(String title, String url);

    //
    // Optional methods for handing events.
    //

    /**
     * Informs that content on the Feed has changed.
     */
    void onContentsChanged();

    /**
     * Informs that the stream was created.
     */
    void onStreamCreated();
}
