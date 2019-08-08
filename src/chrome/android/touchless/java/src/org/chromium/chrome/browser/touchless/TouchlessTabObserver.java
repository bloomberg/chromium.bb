// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.touchless.TouchlessEventHandler;

/**
 * Obsever for touchless mode.
 *
 * Bridges events to TouchlessEventHandler, and maintains the media playing state for the current
 * tab.
 */
public class TouchlessTabObserver extends EmptyTabObserver {
    private MediaSessionObserver mMediaSessionObserver;

    /** Updated by Media Session Observer when the media state changes. */
    private boolean mPlayingMedia;

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        // Only want to observer the navigation in main frame and not fragment
        // navigation.
        if (navigation.isInMainFrame() && !navigation.isFragmentNavigation()) {
            TouchlessEventHandler.onDidFinishNavigation();
        }
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        // Only want to observer the activity hidden.
        if (type == Tab.TabHidingType.ACTIVITY_HIDDEN) {
            TouchlessEventHandler.onActivityHidden();
        }
    }

    @Override
    public void onContentChanged(Tab tab) {
        setWebContents(tab.getWebContents());
    }

    /**
     * @return whether media is playing currently in this tab.
     */
    public boolean isPlayingMedia() {
        return mPlayingMedia;
    }

    private void setWebContents(WebContents webContents) {
        // If the web contents is set but the media session is the same, then we don't want to clear
        // our state since we would not know if the media session is suspended or not.
        MediaSession mediaSession = MediaSession.fromWebContents(webContents);
        if (mMediaSessionObserver != null
                && mediaSession == mMediaSessionObserver.getMediaSession()) {
            return;
        }

        cleanupMediaSessionObserver();
        if (mediaSession != null) {
            mMediaSessionObserver = createMediaSessionObserver(mediaSession);
        }
    }

    private MediaSessionObserver createMediaSessionObserver(MediaSession mediaSession) {
        return new MediaSessionObserver(mediaSession) {
            @Override
            public void mediaSessionDestroyed() {
                cleanupMediaSessionObserver();
            }

            @Override
            public void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
                mPlayingMedia = !isSuspended;
            }
        };
    }

    private void cleanupMediaSessionObserver() {
        if (mMediaSessionObserver != null) mMediaSessionObserver.stopObserving();
        mMediaSessionObserver = null;
        mPlayingMedia = false;
    }
}
