// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.util.Pair;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/**
 * Tests for HostBrowserLauncherParams's WebShareTarget parsing.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HostBrowserLauncherParamsTest {
    /*
     * Test that {@link HostBrowserLauncherParams#createWebShareTargetUriString()} handles adding
     * query parameters to an action url with different path formats.
     */
    @Test
    public void testCreateWebShareTargetUriStringBasic() {
        ArrayList<Pair<String, String>> params = new ArrayList<>();
        params.add(new Pair<>("title", "mytitle"));
        params.add(new Pair<>("foo", "bar"));

        String uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/wst", params);
        Assert.assertEquals("https://www.chromium.org/wst?title=mytitle&foo=bar", uri);

        uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/wst/", params);
        Assert.assertEquals("https://www.chromium.org/wst/?title=mytitle&foo=bar", uri);

        uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/base/wst.html", params);
        Assert.assertEquals("https://www.chromium.org/base/wst.html?title=mytitle&foo=bar", uri);

        uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/base/wst.html/", params);
        Assert.assertEquals("https://www.chromium.org/base/wst.html/?title=mytitle&foo=bar", uri);
    }

    /*
     * Test that {@link HostBrowserLauncherParams#createWebShareTargetUriString()} skips null names
     * or values.
     */
    @Test
    public void testCreateWebShareTargetUriStringNull() {
        ArrayList<Pair<String, String>> params = new ArrayList<>();
        params.add(new Pair<>(null, "mytitle"));
        params.add(new Pair<>("foo", null));
        params.add(new Pair<>("hello", "world"));
        params.add(new Pair<>(null, null));
        // The baseUrl, shareAction and params are checked to be non-null in
        // HostBrowserLauncherParams#extractShareTarget.
        String uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/wst", params);
        Assert.assertEquals("https://www.chromium.org/wst?hello=world", uri);
    }

    /*
     * Test that {@link HostBrowserLauncherParams#createWebShareTargetUriString()} handles replacing
     * the query string of an action url with an existing query.
     */
    @Test
    public void testCreateWebShareTargetClearQuery() {
        ArrayList<Pair<String, String>> params = new ArrayList<>();
        params.add(new Pair<>("hello", "world"));
        params.add(new Pair<>("foobar", "baz"));
        String uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/wst?a=b&c=d", params);
        Assert.assertEquals("https://www.chromium.org/wst?hello=world&foobar=baz", uri);
    }

    /*
     * Test that {@link HostBrowserLauncherParams#createWebShareTargetUriString()} escapes
     * characters.
     */
    @Test
    public void testCreateWebShareTargetEscaping() {
        ArrayList<Pair<String, String>> params = new ArrayList<>();
        params.add(new Pair<>("hello", "world !\"#$%&'()*+,-./0?@[\\]^_a`{}~"));
        params.add(new Pair<>("foo bar", "baz"));
        params.add(new Pair<>("a!\"#$%&'()*+,-./0?@[\\]^_a`{}~", "b"));
        String uri = HostBrowserLauncherParams.createWebShareTargetUriString(
                "https://www.chromium.org/wst%25%20space", params);
        Assert.assertEquals(
                "https://www.chromium.org/wst%25%20space?hello=world+!%22%23%24%25%26'()*%2B%2C-.%2F0%3F%40%5B%5C%5D%5E_a%60%7B%7D~&foo+bar=baz&a!%22%23%24%25%26'()*%2B%2C-.%2F0%3F%40%5B%5C%5D%5E_a%60%7B%7D~=b",
                uri);
    }
}
