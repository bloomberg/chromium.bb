// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Unittests for {@link MinidumpUploadCallable}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class MinidumpUploaderTest {
    /* package */ static final String BOUNDARY = "TESTBOUNDARY";
    /* package */ static final String UPLOAD_CRASH_ID = "IMACRASHID";

    @Rule
    public CrashTestRule mTestRule = new CrashTestRule();
    private File mUploadTestFile;

    /**
     * A HttpURLConnection that performs some basic checks to ensure we are uploading
     * minidumps correctly.
     */
    /* package */ static class TestHttpURLConnection extends HttpURLConnection {
        static final String DEFAULT_EXPECTED_CONTENT_TYPE =
                String.format(MinidumpUploader.CONTENT_TYPE_TMPL, BOUNDARY);
        private final String mExpectedContentType;

        /**
         * The value of the "Content-Type" property if the property has been set.
         */
        private String mContentTypePropertyValue = "";

        public TestHttpURLConnection(URL url) {
            this(url, DEFAULT_EXPECTED_CONTENT_TYPE);
        }

        public TestHttpURLConnection(URL url, String contentType) {
            super(url);
            mExpectedContentType = contentType;
            Assert.assertEquals(MinidumpUploader.CRASH_URL_STRING, url.toString());
        }

        @Override
        public void disconnect() {
            // Check that the "Content-Type" property has been set and the property's value.
            Assert.assertEquals(mExpectedContentType, mContentTypePropertyValue);
        }

        @Override
        public InputStream getInputStream() {
            return new ByteArrayInputStream(ApiCompatibilityUtils.getBytesUtf8(UPLOAD_CRASH_ID));
        }

        @Override
        public OutputStream getOutputStream() {
            return new ByteArrayOutputStream();
        }

        @Override
        public int getResponseCode() {
            return 200;
        }

        @Override
        public String getResponseMessage() {
            return null;
        }

        @Override
        public boolean usingProxy() {
            return false;
        }

        @Override
        public void connect() {}

        @Override
        public void setRequestProperty(String key, String value) {
            if (key.equals("Content-Type")) {
                mContentTypePropertyValue = value;
            }
        }
    }

    /**
     * A HttpURLConnectionFactory that performs some basic checks to ensure we are uploading
     * minidumps correctly.
     */
    /* package */ static class TestHttpURLConnectionFactory implements HttpURLConnectionFactory {
        String mContentType;

        public TestHttpURLConnectionFactory() {
            mContentType = TestHttpURLConnection.DEFAULT_EXPECTED_CONTENT_TYPE;
        }

        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new TestHttpURLConnection(new URL(url), mContentType);
            } catch (IOException e) {
                return null;
            }
        }
    }

    /* package */ static class ErrorCodeHttpURLConnectionFactory
            implements HttpURLConnectionFactory {
        private final int mErrorCode;

        ErrorCodeHttpURLConnectionFactory(int errorCode) {
            mErrorCode = errorCode;
        }

        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            try {
                return new TestHttpURLConnection(new URL(url)) {
                    @Override
                    public int getResponseCode() {
                        return mErrorCode;
                    }
                };
            } catch (IOException e) {
                return null;
            }
        }
    }

    /* package */ static class FailHttpURLConnectionFactory implements HttpURLConnectionFactory {
        @Override
        public HttpURLConnection createHttpURLConnection(String url) {
            Assert.fail();
            return null;
        }
    }

    @Before
    public void setUp() throws IOException {
        mUploadTestFile = new File(mTestRule.getCrashDir(), "crashFile");
        CrashTestRule.setUpMinidumpFile(mUploadTestFile, MinidumpUploaderTest.BOUNDARY);
    }

    @After
    public void tearDown() throws IOException {
        mUploadTestFile.delete();
    }

    // This is a regression test for http://crbug.com/712420
    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWithInvalidMinidumpBoundary() throws Exception {
        // Include an invalid character, '[', in the test string.
        final String boundary = "--InvalidBoundaryWithSpecialCharacter--[";

        CrashTestRule.setUpMinidumpFile(mUploadTestFile, boundary);

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory() {
            { mContentType = ""; }
        };

        MinidumpUploader minidumpUploader = new MinidumpUploader(httpURLConnectionFactory);
        MinidumpUploader.Result result = minidumpUploader.upload(mUploadTestFile);
        Assert.assertTrue(result.isFailure());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testCallWithValidMinidumpBoundary() throws Exception {
        // Include all valid characters in the test string.
        final String boundary = "--0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        final String expectedContentType =
                String.format(MinidumpUploader.CONTENT_TYPE_TMPL, boundary);

        CrashTestRule.setUpMinidumpFile(mUploadTestFile, boundary);

        HttpURLConnectionFactory httpURLConnectionFactory = new TestHttpURLConnectionFactory() {
            { mContentType = expectedContentType; }
        };

        MinidumpUploader minidumpUploader = new MinidumpUploader(httpURLConnectionFactory);
        MinidumpUploader.Result result = minidumpUploader.upload(mUploadTestFile);
        Assert.assertTrue(result.isSuccess());
    }

    @Test
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testReceivingErrorCodes() {
        final int[] errorCodes = {400, 401, 403, 404, 500};

        for (int n = 0; n < errorCodes.length; n++) {
            HttpURLConnectionFactory httpURLConnectionFactory =
                    new ErrorCodeHttpURLConnectionFactory(errorCodes[n]);
            MinidumpUploader minidumpUploader = new MinidumpUploader(httpURLConnectionFactory);
            MinidumpUploader.Result result = minidumpUploader.upload(mUploadTestFile);
            Assert.assertTrue(result.isUploadError());
            Assert.assertEquals(result.errorCode(), errorCodes[n]);
        }
    }
}
