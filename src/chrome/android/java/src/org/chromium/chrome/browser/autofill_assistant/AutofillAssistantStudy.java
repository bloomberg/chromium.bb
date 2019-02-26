// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.graphics.Color;
import android.support.annotation.Nullable;

import org.chromium.components.variations.VariationsAssociatedData;

/** Provides access to finch experiment parameters. */
class AutofillAssistantStudy {
    /** Autofill Assistant Study name. */
    private static final String STUDY_NAME = "AutofillAssistant";
    /** Variation url parameter name. */
    private static final String URL_PARAMETER_NAME = "url";
    /** Variation overlay parameter name. */
    private static final String OVERLAY_PARAMETER_NAME = "overlay_color";

    /** Parameter to change the color of the overlay. Returns null if the parameter is not set or
     * invalid.*/
    @Nullable
    static Integer getOverlayColor() {
        Integer color = null;
        String overlayColor =
                VariationsAssociatedData.getVariationParamValue(STUDY_NAME, OVERLAY_PARAMETER_NAME);
        if (!overlayColor.isEmpty()) {
            try {
                color = Color.parseColor(overlayColor);
            } catch (IllegalArgumentException exception) {
                // ignore, return null
            }
        }
        return color;
    }
}
