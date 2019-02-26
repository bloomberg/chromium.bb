// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;

/**
 * Tests WebApkShareTargetUtil.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {WebApkShareTargetUtilTest.WebApkShareTargetUtilShadow.class})
public class WebApkShareTargetUtilTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME =
            "org.chromium.webapk.class_name";

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";

    @Before
    public void setUp() {
        Bundle appBundle = new Bundle();
        appBundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, appBundle);
    }

    @Implements(WebApkShareTargetUtil.class)
    public static class WebApkShareTargetUtilShadow extends WebApkShareTargetUtil {
        private static Bundle sShareActivityBundle;

        public static void setActivityMetaData(Bundle shareActivityBundle) {
            sShareActivityBundle = shareActivityBundle;
        }

        @Implementation
        public static Bundle computeShareTargetMetaData(
                String apkPackageName, WebApkInfo.ShareData shareData) {
            return sShareActivityBundle;
        }
        @Implementation
        public static byte[] readStringFromContentUri(Uri uri) {
            return "content".getBytes();
        }

        @Implementation
        public static String getFileTypeFromContentUri(Uri uri) {
            return "image/gif";
        }

        @Implementation
        public static String getFileNameFromContentUri(Uri uri) {
            return "filename";
        }
    }

    /**
     * Test that post data is null when the share method is GET.
     */
    @Test
    public void testGET() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "GET");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, "application/x-www-form-urlencoded");
        WebApkShareTargetUtilShadow.setActivityMetaData(shareActivityBundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(null,
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData()));

        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        WebApkShareTargetUtilShadow.setActivityMetaData(shareActivityBundle);
        Assert.assertNotEquals(null,
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData()));
    }

    /**
     * Test that post data for application/x-www-form-urlencoded will contain
     * the correct information in the correct place.
     */
    @Test
    public void testPostUrlEncoded() throws UnsupportedEncodingException {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, "application/x-www-form-urlencoded");
        WebApkShareTargetUtilShadow.setActivityMetaData(shareActivityBundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        intent.putExtra(Intent.EXTRA_SUBJECT, "extra_subject");
        intent.putExtra(Intent.EXTRA_TEXT, "extra_text");
        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());
        Assert.assertNotEquals(null, postData);
        Assert.assertEquals(2, postData.names.length);
        Assert.assertEquals(2, postData.values.length);
        Assert.assertEquals(0, postData.filenames.length);
        Assert.assertEquals(0, postData.types.length);
        Assert.assertFalse(postData.isMultipartEncoding);

        Assert.assertEquals("title", postData.names[0]);
        Assert.assertEquals("text", postData.names[1]);
        Assert.assertEquals("extra_subject", new String(postData.values[0], "UTF-8"));
        Assert.assertEquals("extra_text", new String(postData.values[1], "UTF-8"));
    }

    /**
     * Test that multipart/form-data with no names/accepts output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoNamesNoAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        // Note that names and accepts are not specified
        WebApkShareTargetUtilShadow.setActivityMetaData(shareActivityBundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(null);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(null,
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData()));
    }

    /**
     * Test that multipart/form-data with no files specified in Intent.EXTRA_STREAM will output a
     * null postdata.
     */
    @Test
    public void testPostMultipartWithNoFiles() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        WebApkShareTargetUtilShadow.setActivityMetaData(shareActivityBundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        // Intent.EXTRA_STREAM is not specified.
        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(null,
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData()));
    }

    @Test
    public void testPostMultipartWithFiles() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        WebApkShareTargetUtilShadow.setActivityMetaData(shareActivityBundle);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(null);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());
        Assert.assertNotEquals(null, postData);

        Assert.assertEquals(1, postData.names.length);
        Assert.assertEquals("name", postData.names[0]);

        Assert.assertEquals(1, postData.values.length);
        Assert.assertEquals("content", new String(postData.values[0]));

        Assert.assertEquals(1, postData.filenames.length);
        Assert.assertEquals("filename", postData.filenames[0]);

        Assert.assertEquals(1, postData.types.length);
        Assert.assertEquals("image/gif", postData.types[0]);
    }
}