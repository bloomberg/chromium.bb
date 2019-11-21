// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Class tracking the content IDs associated to a session. */
public class SessionContentTracker implements Dumpable {
  private static final String TAG = "SessionContentTracker";

  private final boolean supportsClearAll;
  private final Set<String> contentIds = new HashSet<>();

  SessionContentTracker(boolean supportsClearAll) {
    this.supportsClearAll = supportsClearAll;
  }

  public boolean isEmpty() {
    return contentIds.isEmpty();
  }

  public void clear() {
    contentIds.clear();
  }

  public boolean contains(String contentId) {
    return contentIds.contains(contentId);
  }

  public Set<String> getContentIds() {
    return new HashSet<>(contentIds);
  }

  public void update(StreamStructure streamStructure) {
    String contentId = streamStructure.getContentId();
    switch (streamStructure.getOperation()) {
      case UPDATE_OR_APPEND: // Fall-through
      case REQUIRED_CONTENT:
        contentIds.add(contentId);
        break;
      case REMOVE:
        contentIds.remove(contentId);
        break;
      case CLEAR_ALL:
        if (supportsClearAll) {
          contentIds.clear();
        } else {
          Logger.i(TAG, "CLEAR_ALL not supported.");
        }
        break;
      default:
        Logger.e(TAG, "unsupported operation: %s", streamStructure.getOperation());
    }
  }

  public void update(List<StreamStructure> streamStructures) {
    for (StreamStructure streamStructure : streamStructures) {
      update(streamStructure);
    }
  }

  @Override
  public void dump(Dumper dumper) {
    dumper.title(TAG);
    dumper.forKey("contentIds").value(contentIds.size());
  }
}
