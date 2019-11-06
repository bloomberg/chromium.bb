// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import org.chromium.chrome.browser.touchless.ui.tooltip.TooltipView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class holds property keys used in key functions IPH {@link PropertyModel}.
 */
public class KeyFunctionsIPHProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_CURSOR_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<TooltipView> TOOLTIP_VIEW =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {IS_CURSOR_VISIBLE, IS_VISIBLE, TOOLTIP_VIEW};
}
