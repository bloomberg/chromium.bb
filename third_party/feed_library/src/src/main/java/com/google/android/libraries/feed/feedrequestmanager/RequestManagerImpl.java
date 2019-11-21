// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedrequestmanager;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.internal.requestmanager.FeedRequestManager;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;

/** Implementation of Client-visible RequestManagerApi. */
public final class RequestManagerImpl implements RequestManager {
  private static final String TAG = "RequestManagerImpl";

  private final FeedRequestManager feedRequestManager;
  private final FeedSessionManager feedSessionManager;

  public RequestManagerImpl(
      FeedRequestManager feedRequestManager, FeedSessionManager feedSessionManager) {
    this.feedRequestManager = feedRequestManager;
    this.feedSessionManager = feedSessionManager;
  }

  @Override
  public void triggerScheduledRefresh() {
    feedRequestManager.triggerRefresh(
        RequestReason.HOST_REQUESTED,
        feedSessionManager.getUpdateConsumer(MutationContext.EMPTY_CONTEXT));
  }
}
