// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previewtab;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.firstrun.DisableFirstRun;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

/**
 * Tests the Preview Tab, also known as the Ephemeral Tab.  Based on the
 * FocusedEditableTextFieldZoomTest and TabsTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
public class PreviewTabTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();

    /** Needed to ensure the First Run Flow is disabled automatically during setUp, etc. */
    @Rule
    public DisableFirstRun mDisableFirstRunFlowRule = new DisableFirstRun();

    private static final String BASE_PAGE = "/chrome/test/data/android/previewtab/base_page.html";
    private static final String PREVIEW_TAB =
            "/chrome/test/data/android/previewtab/preview_tab.html";
    private static final String PREVIEW_TAB_DOM_ID = "previewTab";
    private static final String NEAR_BOTTOM_DOM_ID = "nearBottom";

    private EphemeralTabCoordinator mEphemeralTabCoordinator;
    private BottomSheetTestSupport mSheetTestSupport;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getServer().getURL(BASE_PAGE));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabbedRootUiCoordinator tabbedRootUiCoordinator =
                    ((TabbedRootUiCoordinator) mActivityTestRule.getActivity()
                                    .getRootUiCoordinatorForTesting());
            mEphemeralTabCoordinator =
                    tabbedRootUiCoordinator.getEphemeralTabCoordinatorForTesting();
        });
        mSheetTestSupport = new BottomSheetTestSupport(mActivityTestRule.getActivity()
                                                               .getRootUiCoordinatorForTesting()
                                                               .getBottomSheetController());
    }

    /**
     * End all animations that already started before so that the UI will be in a state ready
     * for the next command.
     */
    private void endAnimations() {
        TestThreadUtils.runOnUiThreadBlocking(mSheetTestSupport::endAllAnimations);
    }

    private void closePreviewTab() {
        TestThreadUtils.runOnUiThreadBlocking(mEphemeralTabCoordinator::close);
        endAnimations();
        Assert.assertFalse("The Preview Tab should have closed but did not indicate closed",
                mEphemeralTabCoordinator.isOpened());
    }

    /**
     * Test bringing up the PT, scrolling the base page but never expanding the PT, then closing it.
     */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    public void testOpenAndClose() throws Throwable {
        Assert.assertFalse("Test should have started without any Preview Tab",
                mEphemeralTabCoordinator.isOpened());

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                activity, tab, PREVIEW_TAB_DOM_ID, R.id.contextmenu_open_in_ephemeral_tab);
        endAnimations();
        Assert.assertTrue("The Preview Tab did not open", mEphemeralTabCoordinator.isOpened());

        // Scroll the base page.
        DOMUtils.scrollNodeIntoView(tab.getWebContents(), NEAR_BOTTOM_DOM_ID);
        endAnimations();
        Assert.assertTrue("The Preview Tab did not stay open after a scroll action",
                mEphemeralTabCoordinator.isOpened());

        closePreviewTab();
    }

    /**
     * Test that closing all incognito tabs successfully handles the base tab and
     * its preview tab opened in incognito mode. This makes sure an incognito profile
     * shared by the tabs is destroyed safely.
     */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    public void testCloseAllIncognitoTabsClosesPreviewTab() throws Throwable {
        Assert.assertFalse("Test should have started without any Preview Tab",
                mEphemeralTabCoordinator.isOpened());

        mActivityTestRule.loadUrlInNewTab(mTestServer.getServer().getURL(BASE_PAGE),
                /*incognito=*/true);
        mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        Assert.assertTrue(tab.isIncognito());

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                activity, tab, PREVIEW_TAB_DOM_ID, R.id.contextmenu_open_in_ephemeral_tab);
        endAnimations();
        BottomSheetController bottomSheet =
                activity.getRootUiCoordinatorForTesting().getBottomSheetController();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheet.expandSheet();
            endAnimations();
            IncognitoUtils.closeAllIncognitoTabs();
            endAnimations();
        });
        Assert.assertEquals(SheetState.HIDDEN, bottomSheet.getSheetState());
    }

    /**
     * Test preview tab suppresses contextual search.
     */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    public void testSuppressContextualSearch() throws Throwable {
        ChromeActivity activity = mActivityTestRule.getActivity();
        ContextualSearchManager csManager = activity.getContextualSearchManager();
        Assert.assertFalse("Contextual Search should be active", csManager.isSuppressed());

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mEphemeralTabCoordinator.requestOpenSheet(
                                new GURL(mTestServer.getServer().getURL(PREVIEW_TAB)), "PreviewTab",
                                false));
        endAnimations();
        Assert.assertTrue("The Preview Tab did not open", mEphemeralTabCoordinator.isOpened());
        Assert.assertTrue("Contextual Search should be suppressed", csManager.isSuppressed());

        closePreviewTab();
        Assert.assertFalse("Contextual Search should be active", csManager.isSuppressed());
    }
}
