// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulatorTest.ShadowUrlUtilities;

/**
 * Unit tests for CachedImageFetcherImpl.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class, BackgroundShadowAsyncTask.class})
public class CachedImageFetcherImplTest {
    private static final String URL = "http://foo.bar";
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;

    CachedImageFetcherImpl mCachedImageFetcher;

    @Mock
    CachedImageFetcherBridge mCachedImageFetcherBridge;
    @Mock
    Bitmap mBitmap;

    @Captor
    ArgumentCaptor<Integer> mWidthCaptor;
    @Captor
    ArgumentCaptor<Integer> mHeightCaptor;
    @Captor
    ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mCachedImageFetcher = Mockito.spy(new CachedImageFetcherImpl(mCachedImageFetcherBridge));
        Mockito.doReturn(URL).when(mCachedImageFetcherBridge).getFilePath(anyObject());
        doAnswer((InvocationOnMock invocation) -> {
            mCallbackCaptor.getValue().onResult(mBitmap);
            return null;
        })
                .when(mCachedImageFetcherBridge)
                .fetchImage(eq(URL), mWidthCaptor.capture(), mHeightCaptor.capture(),
                        mCallbackCaptor.capture());
    }

    @Test
    @SmallTest
    public void testFetchImageWithDimensionsFoundOnDisk() throws Exception {
        Mockito.doReturn(mBitmap).when(mCachedImageFetcher).tryToLoadImageFromDisk(anyObject());
        mCachedImageFetcher.fetchImage(
                URL, WIDTH_PX, HEIGHT_PX, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        verify(mCachedImageFetcher).fetchImageImpl(eq(URL), eq(WIDTH_PX), eq(HEIGHT_PX), any());
        verify(mCachedImageFetcherBridge, never()) // Should never make it to native.
                .fetchImage(eq(URL), anyInt(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void testFetchImageWithDimensionsCallToNative() throws Exception {
        Mockito.doReturn(null).when(mCachedImageFetcher).tryToLoadImageFromDisk(anyObject());
        mCachedImageFetcher.fetchImage(
                URL, WIDTH_PX, HEIGHT_PX, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        verify(mCachedImageFetcher).fetchImageImpl(eq(URL), eq(WIDTH_PX), eq(HEIGHT_PX), any());
        verify(mCachedImageFetcherBridge).fetchImage(eq(URL), anyInt(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void testFetchImageWithNoDimensionsFoundOnDisk() throws Exception {
        Mockito.doReturn(mBitmap).when(mCachedImageFetcher).tryToLoadImageFromDisk(anyObject());
        mCachedImageFetcher.fetchImage(URL, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        verify(mCachedImageFetcher).fetchImageImpl(eq(URL), eq(0), eq(0), any());
        verify(mCachedImageFetcherBridge, never()).fetchImage(eq(URL), anyInt(), anyInt(), any());
    }

    @Test
    @SmallTest
    public void testFetchImageWithNoDimensionsCallToNative() throws Exception {
        Mockito.doReturn(null).when(mCachedImageFetcher).tryToLoadImageFromDisk(anyObject());
        mCachedImageFetcher.fetchImage(URL, (Bitmap bitmap) -> { assertEquals(bitmap, mBitmap); });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        verify(mCachedImageFetcher).fetchImageImpl(eq(URL), eq(0), eq(0), any());
        verify(mCachedImageFetcherBridge).fetchImage(eq(URL), anyInt(), anyInt(), any());
    }
}