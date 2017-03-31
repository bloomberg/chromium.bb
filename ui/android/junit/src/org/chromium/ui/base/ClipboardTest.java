// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests logic in the Clipboard class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClipboardTest {
    @BeforeClass
    public static void beforeClass() {
        RecordHistogram.setDisabledForTests(true);
        RecordUserAction.setDisabledForTests(true);
    }

    @AfterClass
    public static void afterClass() {
        RecordHistogram.setDisabledForTests(false);
        RecordUserAction.setDisabledForTests(false);
    }

    @Test
    public void testGetClipboardContentChangeTimeInMillis() {
        ContextUtils.initApplicationContext(RuntimeEnvironment.application);
        // Upon launch, the clipboard should be at start of epoch, i.e., ancient.
        Clipboard clipboard = Clipboard.getInstance();
        assertEquals(0, clipboard.getClipboardContentChangeTimeInMillis());
        // After updating the clipboard, it should have a new time.
        clipboard.onPrimaryClipChanged();
        assertTrue(clipboard.getClipboardContentChangeTimeInMillis() > 0);
    }
}
