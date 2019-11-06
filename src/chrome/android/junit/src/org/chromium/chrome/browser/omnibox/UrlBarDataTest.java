// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link UrlBarData}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UrlBarDataTest {
    @Test
    public void forUrlAndText_nonHttpOrHttps() {
        UrlBarData data =
                UrlBarData.forUrlAndText("data:text/html,blah,blah", "data:text/html,blah", "BLAH");
        Assert.assertEquals("data:text/html,blah,blah", data.url);
        Assert.assertEquals("data:text/html,blah", data.displayText);
        Assert.assertEquals(0, data.originStartIndex);
        // Ensure that the end index is the length of the display text and not the URL.
        Assert.assertEquals(data.displayText.length(), data.originEndIndex);
    }
}
