// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

/**
 * A DownloadForegroundServiceObservers.Observer implementation for DownloadNotificationService.
 */
public class DownloadNotificationServiceObserver
        implements DownloadForegroundServiceObservers.Observer {
    @Override
    public void onForegroundServiceRestarted(int pinnedNotificationId) {
        DownloadNotificationService.getInstance().onForegroundServiceRestarted(
                pinnedNotificationId);
    }

    @Override
    public void onForegroundServiceTaskRemoved() {
        DownloadNotificationService.getInstance().onForegroundServiceTaskRemoved();
    }

    @Override
    public void onForegroundServiceDestroyed() {
        DownloadNotificationService.getInstance().onForegroundServiceDestroyed();
    }
}
