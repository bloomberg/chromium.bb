// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import static org.chromium.base.test.util.Batch.PER_CLASS;
import static org.chromium.chrome.browser.paint_preview.TabbedPaintPreviewTest.assertAttachedAndShown;

import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the {@link DemoPaintPreview} class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures({ChromeFeatureList.PAINT_PREVIEW_DEMO})
@Batch(PER_CLASS)
public class DemoPaintPreviewTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String TEST_URL = "/chrome/test/data/android/about.html";
    private static PaintPreviewTabService sMockService;

    @BeforeClass
    public static void setUp() {
        sMockService = Mockito.mock(PaintPreviewTabService.class);
        TabbedPaintPreview.overridePaintPreviewTabServiceForTesting(sMockService);
        PlayerManager.overrideCompositorDelegateFactoryForTesting(
                TabbedPaintPreviewTest.TestCompositorDelegate::new);
    }

    @AfterClass
    public static void tearDown() {
        PlayerManager.overrideCompositorDelegateFactoryForTesting(null);
        TabbedPaintPreview.overridePaintPreviewTabServiceForTesting(null);
    }

    @Before
    public void setup() {
        sActivityTestRule.loadUrl(sActivityTestRule.getTestServer().getURL(TEST_URL));
    }

    /**
     * Tests the demo mode is accessible from app menu and works successfully when the page has not
     * been captured before.
     */
    @Test
    @MediumTest
    public void testWithNoExistingCapture() throws UiObjectNotFoundException, ExecutionException {
        // Return false for PaintPreviewTabService#hasCaptureForTab initially.
        Mockito.doReturn(false).when(sMockService).hasCaptureForTab(Mockito.anyInt());

        // When PaintPreviewTabService#captureTab is called, return true for future calls to
        // PaintPreviewTabService#hasCaptureForTab and call the success callback with true.
        ArgumentCaptor<Callback<Boolean>> mCallbackCaptor = ArgumentCaptor.forClass(Callback.class);
        Mockito.doAnswer(invocation -> {
                   Mockito.doReturn(true).when(sMockService).hasCaptureForTab(Mockito.anyInt());
                   mCallbackCaptor.getValue().onResult(true);
                   return null;
               })
                .when(sMockService)
                .captureTab(Mockito.any(Tab.class), mCallbackCaptor.capture());

        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        uiDevice.pressMenu();
        uiDevice.findObject(new UiSelector().text("Show Paint Preview")).click();

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        assertAttachedAndShown(tabbedPaintPreview, true, true);
    }
}
