// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.host.logging.ContentLoggingData;

/**
 * Implementation of {@link BasicLoggingApi} that log actions performed on the Feed.
 */
public class FeedBasicLogging implements BasicLoggingApi {
    // TODO(gangwu): implement BasicLoggingApi functionality.
    @Override
    public void onContentViewed(ContentLoggingData data) {}
    @Override
    public void onContentDismissed(ContentLoggingData data) {}
    @Override
    public void onContentClicked(ContentLoggingData data) {}
    @Override
    public void onContentContextMenuOpened(ContentLoggingData data) {}
    @Override
    public void onMoreButtonViewed(int position) {}
    @Override
    public void onMoreButtonClicked(int position) {}
    @Override
    public void onOpenedWithContent(int timeToPopulateMs, int contentCount) {}
    @Override
    public void onOpenedWithNoImmediateContent() {}
    @Override
    public void onOpenedWithNoContent() {}
}
