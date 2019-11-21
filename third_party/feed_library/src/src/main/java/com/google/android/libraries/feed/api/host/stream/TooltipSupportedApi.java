// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.host.stream;

import com.google.android.libraries.feed.api.host.stream.TooltipInfo.FeatureName;
import com.google.android.libraries.feed.common.functional.Consumer;

/** Interface to communicate if particular types of tooltips meet triggering conditions. */
public interface TooltipSupportedApi {
    /**
     * Checks if the tooltip would be shown if {@link TooltipApi#maybeShowHelpUi} is called.
     *
     * @param featureName the name of the tooltip feature that might be triggered.
     * @param consumer a callback that contains a boolean indicating that the tooltip would be shown
     *     if {@link TooltipApi#maybeShowHelpUi} is called.
     */
    void wouldTriggerHelpUi(@FeatureName String featureName, Consumer<Boolean> consumer);
}
