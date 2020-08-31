// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the load progress bar. Owns all progress bar sub-components.
 */
public class LoadProgressCoordinator {
    private final PropertyModel mModel;
    private final LoadProgressMediator mMediator;
    private final ToolbarProgressBar mProgressBarView;
    private final LoadProgressViewBinder mLoadProgressViewBinder;
    private final PropertyModelChangeProcessor<PropertyModel, ToolbarProgressBar, PropertyKey>
            mPropertyModelChangeProcessor;

    public LoadProgressCoordinator(
            ActivityTabProvider activityTabProvider, ToolbarProgressBar progressBarView) {
        mProgressBarView = progressBarView;
        mModel = new PropertyModel(LoadProgressProperties.ALL_KEYS);
        mMediator = new LoadProgressMediator(activityTabProvider, mModel);
        mLoadProgressViewBinder = new LoadProgressViewBinder();

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mProgressBarView, mLoadProgressViewBinder::bind);
    }
}
