// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class binds the touchless progress bar {@link PropertyModel} to {@link ProgressBarView}.
 */
public class ProgressBarViewBinder {
    public static void bind(
            PropertyModel model, ProgressBarView progressBarView, PropertyKey propertyKey) {
        if (ProgressBarProperties.PROGRESS_FRACTION == propertyKey) {
            progressBarView.setProgress(model.get(ProgressBarProperties.PROGRESS_FRACTION));
        } else if (ProgressBarProperties.IS_ENABLED == propertyKey) {
            if (!model.get(ProgressBarProperties.IS_ENABLED)) {
                progressBarView.setVisibility(false);
            } else if (model.getAllProperties().contains(ProgressBarProperties.IS_VISIBLE)) {
                progressBarView.setVisibility(model.get(ProgressBarProperties.IS_VISIBLE));
            }
        } else if (ProgressBarProperties.IS_VISIBLE == propertyKey) {
            if (!model.getAllProperties().contains(ProgressBarProperties.IS_ENABLED)
                    || model.get(ProgressBarProperties.IS_ENABLED)) {
                progressBarView.setVisibility(model.get(ProgressBarProperties.IS_VISIBLE));
            }
        } else if (ProgressBarProperties.URL == propertyKey) {
            progressBarView.setUrlText(model.get(ProgressBarProperties.URL));
        }
    }
}
