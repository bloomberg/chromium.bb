// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Test for ImageFetcher.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageFetcherTest {
    private static final String URL = "https://www.example.com/image";
    private static final String CLIENT_NAME = "client";
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;
    private static final int EXPIRATION_INTERVAL = 60;

    /**
     * Concrete implementation for testing purposes.
     */
    private class ImageFetcherForTest extends ImageFetcher {
        ImageFetcherForTest(ImageFetcherBridge imageFetcherBridge) {
            super(imageFetcherBridge);
        }

        @Override
        public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {}

        @Override
        public void fetchImage(
                String url, String clientName, int width, int height, Callback<Bitmap> callback) {}

        @Override
        public void fetchImage(Params params, Callback<Bitmap> callback) {}

        @Override
        public void clear() {}

        @Override
        public int getConfig() {
            return 0;
        }

        @Override
        public void destroy() {}
    }

    @Mock
    ImageFetcherBridge mBridge;

    private final Bitmap mBitmap =
            Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);

    private ImageFetcherForTest mImageFetcher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mImageFetcher = Mockito.spy(new ImageFetcherForTest(mBridge));
    }

    @Test
    public void testResize() {
        Bitmap result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX / 2, HEIGHT_PX / 2);
        assertNotEquals(result, mBitmap);
    }

    @Test
    public void testResizeBailsOutIfSizeIsZeroOrLess() {
        Bitmap result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX - 1, HEIGHT_PX - 1);
        assertNotEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, 0, HEIGHT_PX);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX, 0);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, 0, 0);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, -1, HEIGHT_PX);
        assertEquals(result, mBitmap);

        result = ImageFetcher.resizeImage(mBitmap, WIDTH_PX, -1);
        assertEquals(result, mBitmap);
    }

    @Test
    public void testFetchImageNoDimensionsAlias() {
        mImageFetcher.fetchImage(URL, CLIENT_NAME, result -> {});

        // No arguments should alias to 0, 0.
        verify(mImageFetcher)
                .fetchImage(eq(ImageFetcher.Params.create(URL, CLIENT_NAME, 0, 0)), any());
    }

    @Test
    public void testCreateParams() {
        // Verifies params without size specified.
        ImageFetcher.Params params = ImageFetcher.Params.create(URL, CLIENT_NAME);
        assertEquals(URL, params.url);
        assertEquals(CLIENT_NAME, params.clientName);
        assertEquals(0, params.width);
        assertEquals(0, params.height);
        assertEquals(0, params.expirationIntervalMinutes);

        // Verifies params with size.
        params = ImageFetcher.Params.create(URL, CLIENT_NAME, WIDTH_PX, HEIGHT_PX);
        assertEquals(URL, params.url);
        assertEquals(CLIENT_NAME, params.clientName);
        assertEquals(WIDTH_PX, params.width);
        assertEquals(HEIGHT_PX, params.height);
        assertEquals(0, params.expirationIntervalMinutes);
    }

    @Test
    public void testCreateParamsWithExpirationInterval() {
        // Verifies params with expiration interval.
        ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(
                URL, CLIENT_NAME, WIDTH_PX, HEIGHT_PX, EXPIRATION_INTERVAL);
        assertEquals(URL, params.url);
        assertEquals(CLIENT_NAME, params.clientName);
        assertEquals(WIDTH_PX, params.width);
        assertEquals(HEIGHT_PX, params.height);
        assertEquals(EXPIRATION_INTERVAL, params.expirationIntervalMinutes);
    }

    @Test
    public void testParamsEqual() {
        // Different URLs.
        ImageFetcher.Params params1 = ImageFetcher.Params.create(URL, CLIENT_NAME);
        ImageFetcher.Params params2 = ImageFetcher.Params.create("Not the same URL", CLIENT_NAME);
        assertFalse(params1.equals(params2));
        assertFalse(params2.equals(params1));
        assertNotEquals(params1.hashCode(), params2.hashCode());

        // Different width and height.
        params2 = ImageFetcher.Params.create(URL, CLIENT_NAME, WIDTH_PX, HEIGHT_PX);
        assertFalse(params1.equals(params2));
        assertFalse(params2.equals(params1));
        assertNotEquals(params1.hashCode(), params2.hashCode());

        // Different expiration intervals.
        params1 = ImageFetcher.Params.createWithExpirationInterval(
                URL, CLIENT_NAME, WIDTH_PX, HEIGHT_PX, EXPIRATION_INTERVAL);
        assertFalse(params1.equals(params2));
        assertFalse(params2.equals(params1));
        assertNotEquals(params1.hashCode(), params2.hashCode());

        // Same parameters comparison.
        params2 = ImageFetcher.Params.createWithExpirationInterval(
                URL, CLIENT_NAME, WIDTH_PX, HEIGHT_PX, EXPIRATION_INTERVAL);
        assertTrue(params1.equals(params2));
        assertTrue(params2.equals(params1));
        assertEquals(params1.hashCode(), params2.hashCode());

        // Edge cases.
        assertFalse(params1.equals(null));
        assertFalse(params1.equals(new String("Params is not a string.")));
        assertTrue(params1.equals(params1));
    }
}
