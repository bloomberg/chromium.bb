// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.animation;

import android.view.animation.AccelerateInterpolator;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.LinearInterpolator;

import androidx.interpolator.view.animation.FastOutLinearInInterpolator;
import androidx.interpolator.view.animation.FastOutSlowInInterpolator;
import androidx.interpolator.view.animation.LinearOutSlowInInterpolator;

/** Reference to one of each standard interpolator to avoid allocations. */
public class Interpolators {
    public static final AccelerateInterpolator ACCELERATE_INTERPOLATOR =
            new AccelerateInterpolator();
    public static final DecelerateInterpolator DECELERATE_INTERPOLATOR =
            new DecelerateInterpolator();
    public static final FastOutLinearInInterpolator FAST_OUT_LINEAR_IN_INTERPOLATOR =
            new FastOutLinearInInterpolator();
    public static final FastOutSlowInInterpolator FAST_OUT_SLOW_IN_INTERPOLATOR =
            new FastOutSlowInInterpolator();
    public static final LinearInterpolator LINEAR_INTERPOLATOR = new LinearInterpolator();
    public static final LinearOutSlowInInterpolator LINEAR_OUT_SLOW_IN_INTERPOLATOR =
            new LinearOutSlowInInterpolator();
}
