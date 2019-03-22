// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AndroidProtocolHandler;
import org.chromium.base.test.util.Feature;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Test AndroidProtocolHandler.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AndroidProtocolHandlerTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    // star.svg and star.svgz contain the same data. AndroidProtocolHandler should decompress the
    // svgz automatically. Load both from assets and assert that they're equal.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSvgzAsset() throws IOException {
        InputStream svgStream = null;
        InputStream svgzStream = null;
        try {
            svgStream = assertOpen("file:///android_asset/star.svg");
            byte[] expectedData = readFully(svgStream);

            svgzStream = assertOpen("file:///android_asset/star.svgz");
            byte[] actualData = readFully(svgzStream);

            Assert.assertArrayEquals(
                    "Decompressed star.svgz doesn't match star.svg", expectedData, actualData);
        } finally {
            if (svgStream != null) svgStream.close();
            if (svgzStream != null) svgzStream.close();
        }
    }

    private InputStream assertOpen(String url) {
        InputStream stream = AndroidProtocolHandler.open(url);
        Assert.assertNotNull("Failed top open \"" + url + "\"", stream);
        return stream;
    }

    private byte[] readFully(InputStream stream) throws IOException {
        ByteArrayOutputStream data = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        for (;;) {
            int len = stream.read(buf);
            if (len < 1) break;
            data.write(buf, 0, len);
        }
        return data.toByteArray();
    }
}
