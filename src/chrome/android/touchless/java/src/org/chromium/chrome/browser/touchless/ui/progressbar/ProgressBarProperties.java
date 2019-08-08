// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.progressbar;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class holds property keys used in the Loading Bar {@link PropertyModel}.
 */
public class ProgressBarProperties {
    public static final PropertyModel.WritableFloatPropertyKey PROGRESS_FRACTION =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<String> URL =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {PROGRESS_FRACTION, IS_ENABLED, IS_VISIBLE, URL};
}
