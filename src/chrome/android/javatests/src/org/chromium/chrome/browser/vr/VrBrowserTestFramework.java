// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.Assert;

import org.chromium.chrome.test.ChromeActivityTestRule;

/**
 * Extension of VrTestFramework containing VR Browser-specific functionality. Mainly exists in order
 * to preserve the naming convention of using XYZTestFramework for testing feature XYZ.
 */
public class VrBrowserTestFramework extends XrTestFramework {
    public VrBrowserTestFramework(ChromeActivityTestRule rule) {
        super(rule);
        if (!TestVrShellDelegate.isOnStandalone()) {
            Assert.assertFalse("Test started in VR", VrShellDelegate.isInVr());
        }
    }
}
