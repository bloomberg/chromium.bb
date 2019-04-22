// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.compositor.layouts;

import org.junit.rules.ExternalResource;

import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;

/**
 * JUnit 4 rule that disables animations in ChromeAnimation / CompositorAnimationHandler for tests.
 */
public class DisableChromeAnimations extends ExternalResource {
    private boolean mOldTestingMode;
    private float mOldAnimationMultiplier;

    @Override
    protected void before() {
        // old API (ChromeAnimation)
        mOldAnimationMultiplier = ChromeAnimation.Animation.getAnimationMultiplier();
        ChromeAnimation.Animation.setAnimationMultiplierForTesting(0f);
        // new API
        mOldTestingMode = CompositorAnimationHandler.isInTestingMode();
        CompositorAnimationHandler.setTestingMode(true);
    }

    @Override
    protected void after() {
        // old API (ChromeAnimation)
        ChromeAnimation.Animation.setAnimationMultiplierForTesting(mOldAnimationMultiplier);
        // new API
        CompositorAnimationHandler.setTestingMode(mOldTestingMode);
    }
}
