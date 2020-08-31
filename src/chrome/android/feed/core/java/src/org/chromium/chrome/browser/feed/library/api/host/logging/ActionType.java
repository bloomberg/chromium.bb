// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import androidx.annotation.IntDef;

/**
 * IntDef representing the different types of actions.
 *
 * <p>When adding new values, the value of {@link ActionType#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link ActionType#NEXT_VALUE} should not be changed, and those
 * values should not be reused.
 */
@IntDef({ActionType.UNKNOWN, ActionType.OPEN_URL, ActionType.OPEN_URL_INCOGNITO,
        ActionType.OPEN_URL_NEW_TAB, ActionType.OPEN_URL_NEW_WINDOW, ActionType.DOWNLOAD,
        ActionType.LEARN_MORE, ActionType.MANAGE_INTERESTS, ActionType.BLOCK_CONTENT,
        ActionType.NEXT_VALUE})
// LINT.IfChange
public @interface ActionType {
    int UNKNOWN = -1;
    int OPEN_URL = 1;
    int OPEN_URL_INCOGNITO = 2;
    int OPEN_URL_NEW_TAB = 4;
    int OPEN_URL_NEW_WINDOW = 3;
    int DOWNLOAD = 5;
    int LEARN_MORE = 6;
    int MANAGE_INTERESTS = 7;
    int BLOCK_CONTENT = 8;
    int NEXT_VALUE = 9;
}
// LINT.ThenChange
