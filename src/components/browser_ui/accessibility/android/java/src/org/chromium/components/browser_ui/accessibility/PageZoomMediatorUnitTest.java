// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content.browser.HostZoomMapImpl;
import org.chromium.content.browser.HostZoomMapImplJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PageZoomMediator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomMediatorUnitTest {
    // Error messages
    private static final String CURRENT_ZOOM_FAILURE =
            "Failure in fetching the current zoom factor for the model or web contents.";
    private static final String DECREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in decrease zoom method. Expected 1 JNI call but none occurred.";
    private static final String INCREASE_ZOOM_FAILURE_NO_JNI =
            "Failure in increase zoom method. Expected 1 JNI call but none occurred.";
    private static final String DECREASE_ZOOM_FAILURE_WITH_JNI =
            "Failure in decrease zoom method. Expected no JNI calls but at least 1 occurred.";
    private static final String INCREASE_ZOOM_FAILURE_WITH_JNI =
            "Failure in increase zoom method. Expected no JNI calls but at least 1 occurred.";
    private static final String SEEKBAR_VALUE_FAILURE =
            "Failure to update zoom factor or seekbar level when value seekbar value changed.";
    private static final String SEEKBAR_VALUE_FAILURE_NO_JNI =
            "Failure in seekbar value method. Expected 1 JNI call but none occurred.";

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private WebContents mWebContentsMock;

    @Mock
    private HostZoomMapImpl.Natives mHostZoomMapMock;

    private PropertyModel mModel;
    private PageZoomMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(HostZoomMapImplJni.TEST_HOOKS, mHostZoomMapMock);

        mModel = new PropertyModel.Builder(PageZoomProperties.ALL_KEYS).build();
        mMediator = new PageZoomMediator(mModel);
    }

    @Test
    public void testDecreaseZoom() {
        // Verify that calling decrease zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 124, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleDecreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(DECREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 1.56);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 107, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));

        // Verify that when already at the minimum zoom, no value is sent to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(-7.6);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 0, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleDecreaseClicked(null);
        verify(mHostZoomMapMock, never().description(DECREASE_ZOOM_FAILURE_WITH_JNI))
                .setZoomLevel(eq(mWebContentsMock),
                        ArgumentMatchers.doubleThat(argument -> argument < -7.6));
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 0, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testIncreaseZoom() {
        // Verify that calling increase zoom method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 124, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleIncreaseClicked(null);
        verify(mHostZoomMapMock, times(1).description(INCREASE_ZOOM_FAILURE_NO_JNI))
                .setZoomLevel(mWebContentsMock, 3.07);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 150, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));

        // Verify that when already at the maximum zoom, no value is sent to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(8.83);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 475, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleIncreaseClicked(null);
        verify(mHostZoomMapMock, never().description(INCREASE_ZOOM_FAILURE_WITH_JNI))
                .setZoomLevel(eq(mWebContentsMock),
                        ArgumentMatchers.doubleThat(argument -> argument > 8.83));
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 475, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }

    @Test
    public void testSeekBarValueChanged() {
        // Verify the calling seekbar value change method sends expected value to native code.
        when(mHostZoomMapMock.getZoomLevel(any())).thenReturn(2.22);
        mMediator.setWebContents(mWebContentsMock);
        Assert.assertEquals(
                CURRENT_ZOOM_FAILURE, 124, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
        mMediator.handleSeekBarValueChanged(133);
        verify(mHostZoomMapMock, times(1).description(SEEKBAR_VALUE_FAILURE_NO_JNI))
                .setZoomLevel(eq(mWebContentsMock),
                        ArgumentMatchers.doubleThat(
                                argument -> Math.abs(2.5088 - argument) <= 0.001));
        Assert.assertEquals(
                SEEKBAR_VALUE_FAILURE, 133, mModel.get(PageZoomProperties.CURRENT_SEEK_VALUE));
    }
}