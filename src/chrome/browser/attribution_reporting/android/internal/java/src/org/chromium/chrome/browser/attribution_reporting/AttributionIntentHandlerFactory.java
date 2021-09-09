// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.view.InputEvent;

import org.chromium.base.Predicate;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Factory for creating instances of the AttributionIntentHandler from the attribution_reporting
 * module.
 */
public class AttributionIntentHandlerFactory {
    // Static to preserve state across Chrome launches. Preserving state across process kills would
    // also be correct, but probably overkill as we only allow recent InputEvents.
    private static Predicate<InputEvent> sValidator = new InputEventValidator();

    /**
     * @return a AttributionIntentHandler instance.
     */
    public static AttributionIntentHandler create() {
        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)) {
            return new AttributionIntentHandlerImpl(sValidator);
        } else {
            return new NoopAttributionIntentHandler();
        }
    }

    public static void setInputEventValidatorForTesting(Predicate<InputEvent> validator) {
        sValidator = validator;
    }
}
