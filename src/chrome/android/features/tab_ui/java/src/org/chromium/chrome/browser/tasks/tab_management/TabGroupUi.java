// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;

/**
 * Interface for the Tab Groups related UI. This UI manages its own visibility through {@link
 * BottomControlsCoordinator.BottomControlsVisibilityController}.
 */
public interface TabGroupUi {
    void initializeWithNative(ChromeActivity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController);
    void destroy();
}
