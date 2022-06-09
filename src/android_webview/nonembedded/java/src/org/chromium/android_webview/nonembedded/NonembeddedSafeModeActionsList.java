// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import org.chromium.android_webview.common.SafeModeAction;

/**
 * Exposes the SafeModeActions supported by nonembedded Component Updater services.
 */
public final class NonembeddedSafeModeActionsList {
    // Do not instantiate this class.
    private NonembeddedSafeModeActionsList() {}

    /**
     * A list of SafeModeActions supported by nonembedded WebView processes. The set of actions to
     * be executed will be specified by the nonembedded SafeModeService, however each action (if
     * specified by the service) will be executed in the order listed below.
     */
    public static final SafeModeAction[] sList = {
            new ComponentUpdaterResetSafeModeAction(),
    };
}
