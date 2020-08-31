// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import org.junit.Assert;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.base.test.util.Feature;

/**
 * Unit tests for {@link PartnerBrowserCustomizations}.
 */
public class PartnerBrowserCustomizationsUnitTest {
    @CalledByNative
    private PartnerBrowserCustomizationsUnitTest() {}

    @Feature({"Homepage"})
    @CalledByNativeJavaTest
    public void testIsValidHomepage() {
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage(
                "chrome-native://newtab/path#fragment"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("chrome-native://newtab/"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("chrome-native://newtab"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("chrome://newtab"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("chrome:newtab"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("about://newtab"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("about:newtab"));
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage("about:newtab/path#fragment"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("http://example.com"));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage("https:example.com"));

        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("chrome://newtab--not"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("about:newtab--not"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("chrome://history"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("chrome://"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("chrome:"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("chrome"));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage("chrome-native://bookmarks"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("example.com"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(
                "content://com.android.providers.media.documents/document/video:113"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage("ftp://example.com"));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(""));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(null));
    }
}
