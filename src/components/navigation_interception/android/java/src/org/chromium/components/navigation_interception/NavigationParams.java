// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * Navigation parameters container used to keep parameters for navigation interception.
 */
public class NavigationParams {
    /**
     * Target URL of the navigation. Note that this URL is possibly invalid for webview, but
     * otherwise should always be valid.
     */
    public final GURL url;

    /** The referrer URL for the navigation. */
    public final GURL referrer;

    /**
     * The ID of the C++-side NavigationHandle that this instance corresponds to, or 0 if
     * this instance was not constructed from a NavigationHandle.
     */
    public final long navigationId;

    /** True if the the navigation method is "POST". */
    public final boolean isPost;

    /** True if the navigation was initiated by the user. */
    public final boolean hasUserGesture;

    /** Page transition type (e.g. link / typed). */
    public final int pageTransitionType;

    /** Is the navigation a redirect (in which case URL is the "target" address). */
    public final boolean isRedirect;

    /** True if the target URL can't be handled by Chrome's internal protocol handlers. */
    public final boolean isExternalProtocol;

    /** True if the navigation was originated from the main frame. */
    public final boolean isMainFrame;

    /** True if navigation is renderer initiated. Eg clicking on a link. */
    public final boolean isRendererInitiated;

    /** Initiator origin of the request, could be null. */
    public final Origin initiatorOrigin;

    public NavigationParams(GURL url, GURL referrer, long navigationId, boolean isPost,
            boolean hasUserGesture, int pageTransitionType, boolean isRedirect,
            boolean isExternalProtocol, boolean isMainFrame, boolean isRendererInitiated,
            @Nullable Origin initiatorOrigin) {
        this.url = url;
        this.referrer = referrer;
        this.navigationId = navigationId;
        this.isPost = isPost;
        this.hasUserGesture = hasUserGesture;
        this.pageTransitionType = pageTransitionType;
        this.isRedirect = isRedirect;
        this.isExternalProtocol = isExternalProtocol;
        this.isMainFrame = isMainFrame;
        this.isRendererInitiated = isRendererInitiated;
        this.initiatorOrigin = initiatorOrigin;
    }

    @CalledByNative
    public static NavigationParams create(GURL url, GURL referrer, long navigationId,
            boolean isPost, boolean hasUserGesture, int pageTransitionType, boolean isRedirect,
            boolean isExternalProtocol, boolean isMainFrame, boolean isRendererInitiated,
            @Nullable Origin initiatorOrigin) {
        return new NavigationParams(url, referrer, navigationId, isPost, hasUserGesture,
                pageTransitionType, isRedirect, isExternalProtocol, isMainFrame,
                isRendererInitiated, initiatorOrigin);
    }
}
