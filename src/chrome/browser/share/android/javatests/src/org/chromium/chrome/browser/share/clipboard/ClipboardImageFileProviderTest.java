// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.clipboard;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.test.filters.SmallTest;

import androidx.core.content.FileProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.base.Clipboard;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests of {@link ClipboardImageFileProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ClipboardImageFileProviderTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final long WAIT_TIMEOUT_SECONDS = 30L;
    private static final String TEST_PNG_IMAGE_FILE_EXTENSION = ".png";

    private byte[] mTestImageData;

    private class FileProviderHelper implements ContentUriUtils.FileProviderUtil {
        private static final String API_AUTHORITY_SUFFIX = ".FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }

    private class AsyncTaskRunnableHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    private void waitForAsync() throws TimeoutException {
        try {
            AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
            AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
            runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (TimeoutException ex) {
        }
    }

    @Before
    public void setUp() {
        Bitmap bitmap =
                Bitmap.createBitmap(/* width = */ 10, /* height = */ 10, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, /*quality = (0-100) */ 100, baos);
        mTestImageData = baos.toByteArray();

        mActivityTestRule.startMainActivityFromLauncher();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
    }

    @After
    public void tearDown() throws TimeoutException {
        // Clear the clipboard.
        Clipboard.getInstance().setText("");
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1076643")
    public void testClipboardSetImage() throws TimeoutException, IOException {
        Clipboard.getInstance().setImageFileProvider(new ClipboardImageFileProvider());
        Clipboard.getInstance().setImage(mTestImageData, TEST_PNG_IMAGE_FILE_EXTENSION);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return Clipboard.getInstance().getImageUri() != null;
            }
        });

        // Make sure Clipboard::getImage is call on non UI thread.
        AsyncTask.SERIAL_EXECUTOR.execute(
                () -> { Assert.assertNotNull(Clipboard.getInstance().getImage()); });

        // Wait for the above check to complete.
        waitForAsync();
    }
}
