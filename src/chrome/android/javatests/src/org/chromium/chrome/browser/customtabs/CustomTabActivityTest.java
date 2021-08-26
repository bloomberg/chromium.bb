// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.createTestBitmap;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.RemoteViews;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.IntentUtils;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.AppHooksImpl;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.TabsOpenedFromExternalAppTest;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.verification.OriginVerifier;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.OnFinishedForTest;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Instrumentation tests for app menu, context menu, and toolbar of a {@link CustomTabActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabActivityTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final int TIMEOUT_PAGE_LOAD_SECONDS = 10;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String POPUP_PAGE =
            "/chrome/test/data/popup_blocker/popup-window-open.html";
    private static final String SELECT_POPUP_PAGE = "/chrome/test/data/android/select.html";
    private static final String FRAGMENT_TEST_PAGE = "/chrome/test/data/android/fragment.html";
    private static final String TARGET_BLANK_TEST_PAGE =
            "/chrome/test/data/android/cct_target_blank.html";
    private static final String ONLOAD_TITLE_CHANGE = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        window.onload = function () {"
            + "            document.title = \"nytimes.com\";"
            + "        }"
            + "   </script>"
            + "</body></html>";
    private static final String DELAYED_TITLE_CHANGE = "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        window.onload = function () {"
            + "           setTimeout(function (){ document.title = \"nytimes.com\"}, 200);"
            + "        }"
            + "   </script>"
            + "</body></html>";

    private static int sIdToIncrement = 1;

    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;
    private CustomTabsConnection mConnectionToCleanup;

    private class CustomTabsExtraCallbackHelper<T> extends CallbackHelper {
        private T mValue;

        public T getValue() {
            assert getCallCount() > 0;
            return mValue;
        }

        public void notifyCalled(T value) {
            mValue = value;
            notifyCalled();
        }
    }

    @Rule
    public final ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        stopAndShutdownEmbeddedTestServer();

        // finish() is called on a non-UI thread by the testing harness. Must hide the menu
        // first, otherwise the UI is manipulated on a non-UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (getActivity() == null) return;
            AppMenuCoordinator coordinator = mCustomTabActivityTestRule.getAppMenuCoordinator();
            // CCT doesn't always have a menu (ex. in the media viewer).
            if (coordinator == null) return;
            AppMenuHandler handler = coordinator.getAppMenuHandler();
            if (handler != null) handler.hideAppMenu();
        });

        if (mConnectionToCleanup != null) {
            CustomTabsTestUtils.cleanupSessions(mConnectionToCleanup);
        }
    }

    private void stopAndShutdownEmbeddedTestServer() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
            mTestServer = null;
        }
    }

    private TestWebServer createTestWebServer() throws Exception {
        stopAndShutdownEmbeddedTestServer();
        return TestWebServer.start();
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
    }

    @Test
    @SmallTest
    public void testWhitelistedHeadersReceivedWhenConnectionVerified() throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);

        Bundle headers = new Bundle();
        headers.putString("bearer-token", "Some token");
        headers.putString("redirect-url", "https://www.google.com");
        intent.putExtra(Browser.EXTRA_HEADERS, headers);

        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> OriginVerifier.addVerificationOverride("app1",
                                Origin.create(intent.getData()),
                                CustomTabsService.RELATION_USE_AS_ORIGIN));

        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                    assertTrue(params.getVerbatimHeaders().contains("bearer-token: Some token"));
                    assertTrue(params.getVerbatimHeaders().contains(
                            "redirect-url: https://www.google.com"));
                    pageLoadFinishedHelper.notifyCalled();
                }
            });
        });

        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> getSessionDataHolder().handleIntent(intent));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    private void addToolbarColorToIntent(Intent intent, int color) {
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, color);
    }

    private Bundle makeBottomBarBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        PendingIntent pi = PendingIntent.getBroadcast(InstrumentationRegistry.getTargetContext(), 0,
                new Intent(), IntentUtils.getPendingIntentMutabilityFlag(true));

        bundle.putInt(CustomTabsIntent.KEY_ID, sIdToIncrement++);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT, pi);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        return bundle;
    }

    private Bundle makeUpdateVisualsBundle(int id, Bitmap icon, String description) {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, id);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON, icon);
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, description);
        return bundle;
    }

    private void openAppMenuAndAssertMenuShown() {
        CustomTabsTestUtils.openAppMenuAndAssertMenuShown(mCustomTabActivityTestRule.getActivity());
    }

    private Bitmap createVectorDrawableBitmap(@DrawableRes int resId, int widthDp, int heightDp) {
        Context context = InstrumentationRegistry.getTargetContext();
        Drawable vectorDrawable = AppCompatResources.getDrawable(context, resId);
        Bitmap bitmap = createTestBitmap(widthDp, heightDp);
        Canvas canvas = new Canvas(bitmap);
        float density = context.getResources().getDisplayMetrics().density;
        int widthPx = (int) (density * widthDp);
        int heightPx = (int) (density * heightDp);
        vectorDrawable.setBounds(0, 0, widthPx, heightPx);
        vectorDrawable.draw(canvas);
        return bitmap;
    }

    private static boolean isSelectPopupVisible(ChromeActivity activity) {
        Tab tab = activity.getActivityTab();
        if (tab == null || tab.getWebContents() == null) return false;
        return WebContentsUtils.isSelectPopupVisible(tab.getWebContents());
    }

    private void setCanUseHiddenTabForSession(
            CustomTabsConnection connection, CustomTabsSessionToken token, boolean useHiddenTab) {
        assert mConnectionToCleanup == null || mConnectionToCleanup == connection;
        // Save the connection. In case the hidden tab is not consumed by the test, ensure that it
        // is properly cleaned up after the test.
        mConnectionToCleanup = connection;
        connection.setCanUseHiddenTabForSession(token, useHiddenTab);
    }

    /**
     * Test the entries in the context menu shown when long clicking an image.
     * @SmallTest
     * BUG=crbug.com/655970
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForImage() throws TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "logo");
        assertEquals(expectedMenuSize, menu.getCount());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));
    }

    /**
     * Test the entries in the context menu shown when long clicking a link.
     * @SmallTest
     * BUG=crbug.com/655970
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForLink() throws TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "aboutLink");
        assertEquals(expectedMenuSize, menu.getCount());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));
    }

    /**
     * Test the entries in the context menu shown when long clicking an mailto url.
     * @SmallTest
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForMailto() throws TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "email");
        assertEquals(expectedMenuSize, menu.getCount());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));
    }

    /**
     * Test the entries in the context menu shown when long clicking an tel url.
     * @SmallTest
     */
    @Test
    @DisabledTest
    public void testContextMenuEntriesForTel() throws TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());

        final int expectedMenuSize = 12;
        ContextMenuCoordinator menu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "tel");
        assertEquals(expectedMenuSize, menu.getCount());

        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_call));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_send_message));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_add_to_contacts));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_share_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        Assert.assertNotNull(menu.findItem(R.id.contextmenu_save_video));
    }

    @Test
    @SmallTest
    @DisableIf.Build(message = "Fails on Marshmallow, https://crbug.com/1180180",
            sdk_is_greater_than = VERSION_CODES.LOLLIPOP, sdk_is_less_than = VERSION_CODES.N)
    public void
    testContextMenuEntriesBeforeFirstRun() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        // Mark the first run as not completed. This has to be done after we start the intent,
        // otherwise we are going to hit the FRE.
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        // The context menu for images should not be built when the first run is not completed.
        ContextMenuCoordinator imageMenu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "logo");
        assertNull("Context menu for images should not be built when first run is not finished.",
                imageMenu);

        // Options on the context menu for links should be limited when the first run is not
        // completed.
        ContextMenuCoordinator linkMenu = ContextMenuUtils.openContextMenu(
                mCustomTabActivityTestRule.getActivity().getActivityTab(), "aboutLink");
        final int expectedMenuItems = 4;
        Assert.assertEquals("Menu item count does not match expectation.", expectedMenuItems,
                linkMenu.getCount());

        Assert.assertNotNull(linkMenu.findItem(R.id.contextmenu_copy_link_text));
        Assert.assertNotNull(linkMenu.findItem(R.id.contextmenu_copy_link_address));
    }

    /**
     * Test whether a custom tab can be reparented to a new activity.
     */
    @Test
    @SmallTest
    public void testTabReparentingBasic() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.CUSTOM_TAB));
        reparentAndVerifyTab();
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing an infobar.
     */
    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE})
    public void testTabReparentingInfoBar() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestServer.getURL(POPUP_PAGE)));
        CriteriaHelper.pollUiThread(
                () -> isInfoBarSizeOne(mCustomTabActivityTestRule.getActivity().getActivityTab()));

        ChromeActivity newActivity = reparentAndVerifyTab();
        CriteriaHelper.pollUiThread(() -> isInfoBarSizeOne(newActivity.getActivityTab()));
    }

    private static boolean isInfoBarSizeOne(Tab tab) {
        if (tab == null) return false;
        InfoBarContainer container = InfoBarContainer.get(tab);
        if (container == null) return false;
        return container.getInfoBarsForTesting().size() == 1;
    }

    /**
     * Test whether a custom tab can be reparented to a new activity while showing a select popup.
     */
    // @SmallTest
    @Test
    @DisabledTest // Disabled due to flakiness on browser_side_navigation apk - see crbug.com/707766
    public void testTabReparentingSelectPopup() throws TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestServer.getURL(SELECT_POPUP_PAGE)));
        CriteriaHelper.pollUiThread(() -> {
            Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(currentTab, Matchers.notNullValue());
            Criteria.checkThat(currentTab.getWebContents(), Matchers.notNullValue());
        });
        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "select");
        CriteriaHelper.pollUiThread(
                () -> isSelectPopupVisible(mCustomTabActivityTestRule.getActivity()));
        final ChromeActivity newActivity = reparentAndVerifyTab();
        CriteriaHelper.pollUiThread(() -> isSelectPopupVisible(newActivity));
    }
    /**
     * Test whether the color of the toolbar is correctly customized. For L or later releases,
     * status bar color is also tested.
     */
    @Test
    @SmallTest
    @Feature({"StatusBar"})
    public void testToolbarColor() {
        Intent intent = createMinimalCustomTabIntent();
        final int expectedColor = Color.RED;
        addToolbarColorToIntent(intent, expectedColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        assertEquals(expectedColor, toolbar.getBackground().getColor());
        Assert.assertFalse(mCustomTabActivityTestRule.getActivity()
                                   .getToolbarManager()
                                   .getLocationBarModelForTesting()
                                   .shouldEmphasizeHttpsScheme());
        // TODO(https://crbug.com/871805): Use helper class to determine whether dark status icons
        // are supported.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            assertEquals(expectedColor,
                    mCustomTabActivityTestRule.getActivity().getWindow().getStatusBarColor());
        } else {
            assertEquals(ColorUtils.getDarkenedColorForStatusBar(expectedColor),
                    mCustomTabActivityTestRule.getActivity().getWindow().getStatusBarColor());
        }

        MenuButton menuButtonView = toolbarView.findViewById(R.id.menu_button_wrapper);
        assertEquals(menuButtonView.getUseLightDrawablesForTesting(),
                ColorUtils.shouldUseLightForegroundOnBackground(expectedColor));
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void testActionButton() throws TimeoutException {
        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        Intent intent = createMinimalCustomTabIntent();
        final PendingIntent pi = CustomTabsTestUtils.addActionButtonToIntent(
                intent, expectedIcon, "Good test", sIdToIncrement++);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        getActivity()
                .getComponent()
                .resolveToolbarCoordinator()
                .setCustomButtonPendingIntentOnFinishedForTesting(onFinished);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        Assert.assertNotNull(actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue("Action button does not have the correct bitmap.",
                expectedIcon.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        mScreenShooter.shoot("Action Buttons");

        TestThreadUtils.runOnUiThreadBlocking((Runnable) actionButton::performClick);

        onFinished.waitForCallback("Pending Intent was not sent.");
        Assert.assertThat(onFinished.getCallbackIntent().getDataString(), equalTo(mTestPage));
    }

    /**
     * Test if an action button is shown with correct image and size, and clicking it sends the
     * correct {@link PendingIntent}.
     */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void testMultipleActionButtons() throws TimeoutException {
        Bitmap expectedIcon1 = createVectorDrawableBitmap(R.drawable.ic_content_copy_black, 48, 48);
        Bitmap expectedIcon2 =
                createVectorDrawableBitmap(R.drawable.ic_email_googblue_36dp, 48, 48);
        Intent intent = createMinimalCustomTabIntent();

        // Mark the intent as trusted so it can show more than one action button.
        IntentUtils.addTrustedIntentExtras(intent);
        Assert.assertTrue(IntentHandler.wasIntentSenderChrome(intent));

        ArrayList<Bundle> toolbarItems = new ArrayList<>(2);
        final PendingIntent pi1 =
                PendingIntent.getBroadcast(InstrumentationRegistry.getTargetContext(), 0,
                        new Intent(), IntentUtils.getPendingIntentMutabilityFlag(true));
        final OnFinishedForTest onFinished1 = new OnFinishedForTest(pi1);
        toolbarItems.add(CustomTabsTestUtils.makeToolbarItemBundle(
                expectedIcon1, "Good test", pi1, sIdToIncrement++));
        final PendingIntent pi2 =
                PendingIntent.getBroadcast(InstrumentationRegistry.getTargetContext(), 1,
                        new Intent(), IntentUtils.getPendingIntentMutabilityFlag(true));
        Assert.assertThat(pi2, not(equalTo(pi1)));
        final OnFinishedForTest onFinished2 = new OnFinishedForTest(pi2);
        toolbarItems.add(CustomTabsTestUtils.makeToolbarItemBundle(
                expectedIcon2, "Even gooder test", pi2, sIdToIncrement++));
        intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, toolbarItems);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        // Forward the onFinished event to both objects.
        getActivity()
                .getComponent()
                .resolveToolbarCoordinator()
                .setCustomButtonPendingIntentOnFinishedForTesting(
                (pendingIntent, openedIntent, resultCode, resultData, resultExtras) -> {
                    onFinished1.onSendFinished(
                            pendingIntent, openedIntent, resultCode, resultData, resultExtras);
                    onFinished2.onSendFinished(
                            pendingIntent, openedIntent, resultCode, resultData, resultExtras);
                });

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(1);

        Assert.assertNotNull("Action button not found", actionButton);
        Assert.assertNotNull(actionButton.getDrawable());
        Assert.assertTrue("Action button's background is not a BitmapDrawable.",
                actionButton.getDrawable() instanceof BitmapDrawable);

        Assert.assertTrue("Action button does not have the correct bitmap.",
                expectedIcon1.sameAs(((BitmapDrawable) actionButton.getDrawable()).getBitmap()));

        mScreenShooter.shoot("Multiple Action Buttons");

        TestThreadUtils.runOnUiThreadBlocking((Runnable) actionButton::performClick);

        onFinished1.waitForCallback("Pending Intent was not sent.");
        Assert.assertThat(onFinished1.getCallbackIntent().getDataString(), equalTo(mTestPage));
        assertNull(onFinished2.getCallbackIntent());

        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        int id = toolbarItems.get(0).getInt(CustomTabsIntent.KEY_ID);
        Bundle updateActionButtonBundle =
                makeUpdateVisualsBundle(id, expectedIcon2, "Bestest testest");
        Bundle updateVisualsBundle = new Bundle();
        updateVisualsBundle.putParcelableArrayList(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS,
                new ArrayList<>(Arrays.asList(updateActionButtonBundle)));
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.updateVisuals(token, updateVisualsBundle));

        assertEquals("Bestest testest", actionButton.getContentDescription());
    }

    /**
     * Test the case that the action button should not be shown, given a bitmap with unacceptable
     * height/width ratio.
     */
    @Test
    @SmallTest
    public void testActionButtonBadRatio() {
        Bitmap expectedIcon = createTestBitmap(60, 20);
        Intent intent = createMinimalCustomTabIntent();
        CustomTabsTestUtils.setShareState(intent, CustomTabsIntent.SHARE_STATE_OFF);
        CustomTabsTestUtils.addActionButtonToIntent(
                intent, expectedIcon, "Good test", sIdToIncrement++);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        Assert.assertTrue(
                "A custom tab toolbar is never shown", toolbarView instanceof CustomTabToolbar);
        CustomTabToolbar toolbar = (CustomTabToolbar) toolbarView;
        final ImageButton actionButton = toolbar.getCustomActionButtonForTest(0);

        assertNull("Action button should not be shown", actionButton);

        BrowserServicesIntentDataProvider dataProvider = getActivity().getIntentDataProvider();
        Assert.assertThat(dataProvider.getCustomButtonsOnToolbar(), is(empty()));
    }

    @Test
    @SmallTest
    public void testBottomBar() {
        final int numItems = 3;
        final Bitmap expectedIcon = createTestBitmap(48, 24);
        final int barColor = Color.GREEN;

        Intent intent = createMinimalCustomTabIntent();
        ArrayList<Bundle> bundles = new ArrayList<>();
        for (int i = 1; i <= numItems; i++) {
            Bundle bundle = makeBottomBarBundle(i, expectedIcon, Integer.toString(i));
            bundles.add(bundle);
        }
        intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, bundles);
        intent.putExtra(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR, barColor);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        ViewGroup bottomBar = mCustomTabActivityTestRule.getActivity().findViewById(
                R.id.custom_tab_bottom_bar_wrapper);
        Assert.assertNotNull(bottomBar);
        Assert.assertTrue("Bottom Bar wrapper is not visible.",
                bottomBar.getVisibility() == View.VISIBLE && bottomBar.getHeight() > 0
                        && bottomBar.getWidth() > 0);
        assertEquals("Bottom Bar showing incorrect number of buttons.", numItems,
                bottomBar.getChildCount());
        assertEquals("Bottom bar not showing correct color", barColor,
                ((ColorDrawable) bottomBar.getBackground()).getColor());
        for (int i = 0; i < numItems; i++) {
            ImageButton button = (ImageButton) bottomBar.getChildAt(i);
            Assert.assertTrue("Bottom Bar button does not have the correct bitmap.",
                    expectedIcon.sameAs(((BitmapDrawable) button.getDrawable()).getBitmap()));
            Assert.assertTrue("Bottom Bar button is not visible.",
                    button.getVisibility() == View.VISIBLE && button.getHeight() > 0
                            && button.getWidth() > 0);
            assertEquals("Bottom Bar button does not have correct content description",
                    Integer.toString(i + 1), button.getContentDescription());
        }
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void testRemoteViews() {
        Intent intent = createMinimalCustomTabIntent();

        Bitmap expectedIcon = createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 77, 48);
        PendingIntent pi = CustomTabsTestUtils.addActionButtonToIntent(
                intent, expectedIcon, "Good test", sIdToIncrement++);

        // Create a RemoteViews. The layout used here is pretty much arbitrary, but with the
        // constraint that a) it already exists in production code, and b) it only contains views
        // with the @RemoteView annotation.
        RemoteViews remoteViews =
                new RemoteViews(InstrumentationRegistry.getTargetContext().getPackageName(),
                        R.layout.share_sheet_item);
        remoteViews.setTextViewText(R.id.text, "Kittens!");
        remoteViews.setTextViewText(R.id.display_new, "So fluffy");
        remoteViews.setImageViewResource(R.id.icon, R.drawable.ic_email_googblue_36dp);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS, remoteViews);
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS, new int[] {R.id.icon});
        PendingIntent pi2 = PendingIntent.getBroadcast(InstrumentationRegistry.getTargetContext(),
                0, new Intent(), IntentUtils.getPendingIntentMutabilityFlag(true));
        intent.putExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT, pi2);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        mScreenShooter.shoot("Remote Views");
    }

    @Test
    @SmallTest
    public void testLaunchWithSession() throws Exception {
        CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession();
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
    }

    @Test
    @SmallTest
    public void testLoadNewUrlWithSession() throws Exception {
        final Context context = InstrumentationRegistry.getTargetContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        Assert.assertFalse("CustomTabContentHandler handled intent with wrong session",
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return getSessionDataHolder().handleIntent(
                            CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
                }));
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                    is(mTestPage));
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    intent.setData(Uri.parse(mTestPage2));
                    return getSessionDataHolder().handleIntent(intent);
                }));
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    pageLoadFinishedHelper.notifyCalled();
                }
            });
        });
        pageLoadFinishedHelper.waitForCallback(0);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    ChromeTabUtils.getUrlStringOnUiThread(getActivity().getActivityTab()),
                    is(mTestPage2));
        });
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1148544")
    public void testCreateNewTab() throws Exception {
        final String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/customtabs/test_window_open.html");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), testUrl));
        final TabModelSelector tabSelector =
                mCustomTabActivityTestRule.getActivity().getTabModelSelector();

        final CallbackHelper openTabHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tabSelector.getModel(false).addObserver(new TabModelObserver() {
                @Override
                public void didAddTab(
                        Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                    openTabHelper.notifyCalled();
                }
            });
        });
        DOMUtils.clickNode(mCustomTabActivityTestRule.getWebContents(), "new_window");

        openTabHelper.waitForCallback(0, 1);
        assertEquals(
                "A new tab should have been created.", 2, tabSelector.getModel(false).getCount());
    }

    @Test
    @SmallTest
    public void testReferrerAddedAutomatically() throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        String packageName = context.getPackageName();
        final String referrer =
                IntentHandler.constructValidReferrerForAuthority(packageName).getUrl();
        assertEquals(referrer, connection.getDefaultReferrerForSession(session).getUrl());

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                    assertEquals(referrer, params.getReferrer().getUrl());
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    pageLoadFinishedHelper.notifyCalled();
                }
            });
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> getSessionDataHolder()
                                .handleIntent(intent)));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testVerifiedReferrer() throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation()
                                        .getTargetContext()
                                        .getApplicationContext();
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2);
        String referrer = "https://example.com";
        intent.putExtra(Intent.EXTRA_REFERRER_NAME, referrer);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, "app1");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> OriginVerifier.addVerificationOverride("app1", Origin.create(referrer),
                        CustomTabsService.RELATION_USE_AS_ORIGIN));

        final CustomTabsSessionToken session = warmUpAndLaunchUrlWithSession(intent);
        assertEquals(getActivity().getIntentDataProvider().getSession(), session);

        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                    assertEquals(referrer, params.getReferrer().getUrl());
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    pageLoadFinishedHelper.notifyCalled();
                }
            });
        });
        Assert.assertTrue("CustomTabContentHandler can't handle intent with same session",
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> getSessionDataHolder()
                                .handleIntent(intent)));
        pageLoadFinishedHelper.waitForCallback(0);
    }

    /**
     * Tests that the navigation callbacks are sent.
     */
    @Test
    @SmallTest
    public void testCallbacksAreSent() throws Exception {
        final Semaphore navigationStartSemaphore = new Semaphore(0);
        final Semaphore navigationFinishedSemaphore = new Semaphore(0);
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                Assert.assertNotEquals(CustomTabsCallback.NAVIGATION_FAILED, navigationEvent);
                Assert.assertNotEquals(CustomTabsCallback.NAVIGATION_ABORTED, navigationEvent);
                if (navigationEvent == CustomTabsCallback.NAVIGATION_STARTED) {
                    navigationStartSemaphore.release();
                } else if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) {
                    navigationFinishedSemaphore.release();
                }
            }
        }).session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(
                navigationStartSemaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
        Assert.assertTrue(navigationFinishedSemaphore.tryAcquire(
                TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));
    }

    /**
     * Tests that page load metrics are sent.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "crbug/1216498")
    public void testPageLoadMetricsAreSent() throws Exception {
        checkPageLoadMetrics(true);
    }

    /**
     * Tests that page load metrics are not sent when the client is not allowlisted.
     */
    @Test
    @MediumTest
    public void testPageLoadMetricsAreNotSentByDefault() throws Exception {
        checkPageLoadMetrics(false);
    }

    private static void assertSuffixedHistogramTotalCount(long expected, String histogramPrefix) {
        for (String suffix : new String[] {".ZoomedIn", ".ZoomedOut"}) {
            assertEquals(expected,
                    RecordHistogram.getHistogramTotalCountForTesting(histogramPrefix + suffix));
        }
    }

    /**
     * Tests that one navigation in a custom tab records the histograms reflecting time from
     * intent to first navigation start/commit.
     */
    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS})
    public void testNavigationHistogramsRecorded() throws Exception {
        String startHistogramPrefix = "CustomTabs.IntentToFirstNavigationStartTime";
        String commitHistogramPrefix = "CustomTabs.IntentToFirstCommitNavigationTime3";
        assertSuffixedHistogramTotalCount(0, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(0, commitHistogramPrefix);

        final Semaphore semaphore = new Semaphore(0);
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                if (navigationEvent == CustomTabsCallback.NAVIGATION_FINISHED) semaphore.release();
            }
        }).session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_PAGE_LOAD_SECONDS, TimeUnit.SECONDS));

        assertSuffixedHistogramTotalCount(1, startHistogramPrefix);
        assertSuffixedHistogramTotalCount(1, commitHistogramPrefix);
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set onload.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitle() throws Exception {
        TestWebServer webServer = createTestWebServer();
        final String url = webServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, false, "nytimes.com");
        webServer.shutdown();
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set during prerendering.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithProperTitleHiddenTab() throws Exception {
        TestWebServer webServer = createTestWebServer();
        final String url = webServer.setResponse("/test.html", ONLOAD_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, true, "nytimes.com");
        webServer.shutdown();
    }

    /**
     * Tests that TITLE_ONLY state works as expected with a title getting set delayed after load.
     */
    @Test
    @SmallTest
    public void testToolbarTitleOnlyStateWithDelayedTitle() throws Exception {
        TestWebServer webServer = createTestWebServer();
        final String url = webServer.setResponse("/test.html", DELAYED_TITLE_CHANGE, null);
        hideDomainAndEnsureTitleIsSet(url, false, "nytimes.com");
        webServer.shutdown();
    }

    private void hideDomainAndEnsureTitleIsSet(
            final String url, boolean useHiddenTab, final String expectedTitle) throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setHideDomainForSession(token, true);

        if (useHiddenTab) {
            setCanUseHiddenTabForSession(connection, token, true);
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, url);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollUiThread(() -> {
            final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(url));
        });
        CriteriaHelper.pollUiThread(() -> {
            CustomTabToolbar toolbar =
                    mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
            TextView titleBar = toolbar.findViewById(R.id.title_bar);
            Criteria.checkThat(titleBar, Matchers.notNullValue());
            Criteria.checkThat(titleBar.isShown(), is(true));
            Criteria.checkThat(titleBar.getText().toString(), is(expectedTitle));
        });
    }

    /**
     * Tests that when we use a pre-created renderer, the page loaded is the
     * only one in the navigation history.
     */
    @Test
    @SmallTest
    public void testPrecreatedRenderer() throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        // Forcing no hidden tab implies falling back to simply creating a spare WebContents.
        setCanUseHiddenTabForSession(connection, token, false);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(() -> {
            final Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
        });

        Assert.assertFalse(mCustomTabActivityTestRule.getActivity().getActivityTab().canGoBack());
        Assert.assertFalse(
                mCustomTabActivityTestRule.getActivity().getActivityTab().canGoForward());

        List<HistoryItem> history =
                getHistory(mCustomTabActivityTestRule.getActivity().getActivityTab());
        assertEquals(1, history.size());
        assertEquals(mTestPage, history.get(0).getUrl().getSpec());
    }

    /** Tests that calling warmup() is optional without prerendering. */
    @Test
    @SmallTest
    public void testMayLaunchUrlWithoutWarmupNoSpeculation() {
        mayLaunchUrlWithoutWarmup(false);
    }

    /** Tests that calling mayLaunchUrl() without warmup() succeeds. */
    @Test
    @SmallTest
    public void testMayLaunchUrlWithoutWarmupHiddenTab() {
        mayLaunchUrlWithoutWarmup(true);
    }

    /**
     * Tests that launching a regular Chrome tab after warmup() gives the right layout.
     *
     * About the restrictions and switches: No FRE and no document mode to get a
     * ChromeTabbedActivity, and no tablets to have the tab switcher button.
     *
     * Non-regression test for crbug.com/547121.
     * @SmallTest
     * Disabled for flake: https://crbug.com/692025.
     */
    @Test
    @DisabledTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRegularChrome() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        Intent intent = new Intent(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        ChromeTabbedActivity.class.getName(), null, false);
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertNotNull("Main activity did not start", activity);
        ChromeTabbedActivity tabbedActivity =
                (ChromeTabbedActivity) monitor.waitForActivityWithTimeout(
                        ChromeActivityTestRule.getActivityStartTimeoutMs());
        Assert.assertNotNull("ChromeTabbedActivity did not start", tabbedActivity);
        Assert.assertNotNull("Should have a tab switcher button.",
                tabbedActivity.findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests that launching a Custom Tab after warmup() gives the right layout.
     *
     * Non-regression test for crbug.com/547121.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testWarmupAndLaunchRightToolbarLayout() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mCustomTabActivityTestRule.startActivityCompletely(createMinimalCustomTabIntent());
        assertNull("Should not have a tab switcher button.",
                getActivity().findViewById(R.id.tab_switcher_button));
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * The expected behavior is that the hidden tab shouldn't be dropped, and that the fragment is
     * updated.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabAndChangingFragmentIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(true, true);
    }

    /** Same as above, but the hidden tab matching should not ignore the fragment. */
    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1237331")
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabAndChangingFragmentDontIgnoreFragments() throws Exception {
        startHiddenTabAndChangeFragment(false, true);
    }

    /** Same as above, hidden tab matching ignores the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @DisabledTest(message = "https://crbug.com/1148544")
    public void testHiddenTabAndChangingFragmentDontWait() throws Exception {
        startHiddenTabAndChangeFragment(true, false);
    }

    /** Same as above, hidden tab matching doesn't ignore the fragment, don't wait. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabAndChangingFragmentDontWaitDrop() throws Exception {
        startHiddenTabAndChangeFragment(false, false);
    }

    @Test
    @SmallTest
    public void testParallelRequest() throws Exception {
        String url = mTestServer.getURL("/echoheader?Cookie");
        Uri requestUri = Uri.parse(mTestServer.getURL("/set-cookie?acookie"));

        final Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        // warmup(), create session, allow parallel requests, allow origin.
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final Origin origin = Origin.create(requestUri);
        Assert.assertTrue(connection.newSession(token));
        connection.mClientManager.setAllowParallelRequestForSession(token, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerificationOverride(
                    context.getPackageName(), origin, CustomTabsService.RELATION_USE_AS_ORIGIN);
        });

        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, requestUri);
        intent.putExtra(
                CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, Uri.parse(origin.toString()));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        assertEquals("\"acookie\"", content);
    }

    /**
     * Tests the following scenario:
     * - warmup() + mayLaunchUrl("http://example.com/page.html#first-fragment")
     * - loadUrl("http://example.com/page.html#other-fragment")
     *
     * There are two parameters changing the bahavior:
     * @param ignoreFragments Whether the hidden tab should be kept.
     * @param wait Whether to wait for the hidden tab to load.
     */
    private void startHiddenTabAndChangeFragment(boolean ignoreFragments, boolean wait)
            throws Exception {
        String testUrl = mTestServer.getURL(FRAGMENT_TEST_PAGE);
        String initialFragment = "#test";
        String initialUrl = testUrl + initialFragment;
        String fragment = "#yeah";
        String urlWithFragment = testUrl + fragment;

        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, urlWithFragment);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        connection.setIgnoreUrlFragmentsForSession(token, ignoreFragments);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(initialUrl), null, null));

        if (wait) {
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, initialUrl);
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = getActivity().getActivityTab();

        if (wait) {
            ElementContentCriteria initialVisibilityCriteria = new ElementContentCriteria(
                    tab, "visibility", ignoreFragments ? "hidden" : "visible");
            ElementContentCriteria initialFragmentCriteria = new ElementContentCriteria(
                    tab, "initial-fragment", ignoreFragments ? initialFragment : fragment);
            ElementContentCriteria fragmentCriteria =
                    new ElementContentCriteria(tab, "fragment", fragment);
            // The tab hasn't been reloaded.
            CriteriaHelper.pollInstrumentationThread(initialVisibilityCriteria, 2000, 200);
            // No reload (initial fragment is correct).
            CriteriaHelper.pollInstrumentationThread(initialFragmentCriteria, 2000, 200);
            if (ignoreFragments) {
                CriteriaHelper.pollInstrumentationThread(fragmentCriteria, 2000, 200);
            }
        } else {
            CriteriaHelper.pollInstrumentationThread(new ElementContentCriteria(
                    tab, "initial-fragment", fragment), 2000, 200);
        }

        Assert.assertFalse(tab.canGoForward());
        Assert.assertFalse(tab.canGoBack());

        // TODO(ahemery):
        // Fragment misses will trigger two history entries
        // - url#speculated and url#actual are both inserted
        // This should ideally not be the case.
    }

    /**
     * Test whether the url shown on hidden tab gets updated from about:blank when it
     * completes in the background.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabCorrectUrl() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, mTestPage);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        assertEquals(Uri.parse(mTestPage).getHost() + ":" + Uri.parse(mTestPage).getPort(),
                ((EditText) getActivity().findViewById(R.id.url_bar)).getText().toString());
    }

    /**
     * Test that hidden tab speculation is not performed if 3rd party cookies are blocked.
     **/
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    public void testHiddenTabThirdPartyCookiesBlocked() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        connection.warmup(0);

        // Needs the browser process to be initialized.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefs = UserPrefs.get(Profile.getLastUsedRegularProfile());
            int old_block_pref = prefs.getInteger(COOKIE_CONTROLS_MODE);
            prefs.setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.OFF);
            Assert.assertTrue(connection.maySpeculate(token));
            prefs.setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.BLOCK_THIRD_PARTY);
            Assert.assertFalse(connection.maySpeculate(token));
            prefs.setInteger(COOKIE_CONTROLS_MODE, old_block_pref);
        });
    }

    /**
     * Test whether invalid urls are avoided for hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabInvalidUrl() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertFalse(
                connection.mayLaunchUrl(token, Uri.parse("chrome://version"), null, null));
    }

    /**
     * Tests that the activity knows there is already a child process when warmup() has been called.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAllocateChildConnectionWithWarmup() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(
                        "Warmup() should have allocated a child connection",
                        getActivity().shouldAllocateChildConnection()));
    }

    /**
     * Tests that the activity knows there is no child process.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAllocateChildConnectionNoWarmup() {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsConnection.getInstance();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage2));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertTrue(
                        "No spare renderer available, should allocate a child connection.",
                        getActivity().shouldAllocateChildConnection()));
    }

    /**
     * Tests that the activity knows there is already a child process with a hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testAllocateChildConnectionWithHiddenTab() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(
                        "Prerendering should have allocated a child connection",
                        getActivity().shouldAllocateChildConnection()));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRecreateSpareRendererOnTabClose() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsTestUtils.warmUpAndWait();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
            final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
            activity.getComponent().resolveNavigationController().finish(FinishReason.OTHER);
        });
        CriteriaHelper.pollUiThread(()
                                            -> WarmupManager.getInstance().hasSpareWebContents(),
                "No new spare renderer", 2000, 200);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is not lost when launching the
     * Custom Tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabWithReferrer() throws Exception {
        String referrer = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, referrer, referrer);

        Tab tab = getActivity().getActivityTab();
        // The tab hasn't been reloaded.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "hidden"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrer), 2000, 200);
    }

    /**
     * Tests that hidden tab accepts a referrer, and that this is dropped when the tab
     * is launched with a mismatched referrer.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabWithMismatchedReferrers() throws Exception {
        String prerenderReferrer = "android-app://com.foo.me/";
        String launchReferrer = "android-app://com.foo.me.i.changed.my.mind/";
        maybeSpeculateAndLaunchWithReferrers(
                mTestServer.getURL(FRAGMENT_TEST_PAGE), true, prerenderReferrer, launchReferrer);

        Tab tab = getActivity().getActivityTab();
        // Prerender has been dropped.
        CriteriaHelper.pollInstrumentationThread(
                new ElementContentCriteria(tab, "visibility", "visible"), 2000, 200);
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, launchReferrer), 2000, 200);
    }

    /** Tests that a client can set a referrer, without speculating. */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testClientCanSetReferrer() throws Exception {
        String referrerUrl = "android-app://com.foo.me/";
        maybeSpeculateAndLaunchWithReferrers(mTestPage, false, null, referrerUrl);

        Tab tab = getActivity().getActivityTab();
        // The Referrer is correctly set.
        CriteriaHelper.pollInstrumentationThread(
                new TabsOpenedFromExternalAppTest.ReferrerCriteria(tab, referrerUrl), 2000, 200);
    }

    @Test
    @MediumTest
    public void testLaunchIncognitoURL() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final CustomTabActivity cctActivity = mCustomTabActivityTestRule.getActivity();
        final CallbackHelper mCctHiddenCallback = new CallbackHelper();
        final CallbackHelper mTabbedModeShownCallback = new CallbackHelper();
        final AtomicReference<ChromeTabbedActivity> tabbedActivity = new AtomicReference<>();

        ActivityStateListener listener = (activity, newState) -> {
            if (activity == cctActivity
                    && (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED)) {
                mCctHiddenCallback.notifyCalled();
            }

            if (activity instanceof ChromeTabbedActivity && newState == ActivityState.RESUMED) {
                mTabbedModeShownCallback.notifyCalled();
                tabbedActivity.set((ChromeTabbedActivity) activity);
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.registerStateListenerForAllActivities(listener));

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            TabTestUtils.openNewTab(cctActivity.getActivityTab(), new GURL("about:blank"), null,
                    null, WindowOpenDisposition.OFF_THE_RECORD, false);
        });

        mCctHiddenCallback.waitForCallback("CCT not hidden.", 0);
        mTabbedModeShownCallback.waitForCallback("Tabbed mode not shown.", 0);

        CriteriaHelper.pollUiThread(() -> tabbedActivity.get().areTabModelsInitialized());

        CriteriaHelper.pollUiThread(() -> {
            Tab tab = tabbedActivity.get().getActivityTab();
            Criteria.checkThat("Tab is null", tab, Matchers.notNullValue());
            Criteria.checkThat("Incognito tab not selected", tab.isIncognito(), is(true));
            Criteria.checkThat("Wrong URL loaded in incognito tab",
                    ChromeTabUtils.getUrlStringOnUiThread(tab), is("about:blank"));
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> ApplicationStatus.unregisterActivityStateListener(listener));
    }

    /** Maybe prerenders a URL with a referrer, then launch it with another one. */
    private void maybeSpeculateAndLaunchWithReferrers(String url, boolean useHiddenTab,
            String speculationReferrer, String launchReferrer) throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        if (useHiddenTab) {
            CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            connection.newSession(token);
            setCanUseHiddenTabForSession(connection, token, true);
            Bundle extras = null;
            if (speculationReferrer != null) {
                extras = new Bundle();
                extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(speculationReferrer));
            }
            Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), extras, null));
            CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, url);
        }

        if (launchReferrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(launchReferrer));
        }
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    /**
     * Test intended to verify the way we test history is correct.
     * Start an activity and then navigate to a different url.
     * We test NavigationController behavior through canGoBack/Forward as well
     * as browser history through an HistoryProvider.
     */
    @Test
    @SmallTest
    public void testHistoryNoSpeculation() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        final Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> tab.loadUrl(new LoadUrlParams(mTestPage2)));
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestPage2);

        Assert.assertTrue(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory(tab);
        assertEquals(2, history.size());
        assertEquals(mTestPage2, history.get(0).getUrl().getSpec());
        assertEquals(mTestPage, history.get(1).getUrl().getSpec());
    }

    /**
     * The following test that history only has a single final page after speculation,
     * whether it was a hit or a miss.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterHiddenTabHit() throws Exception {
        verifyHistoryAfterHiddenTab(true);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHistoryAfterHiddenTabMiss() throws Exception {
        verifyHistoryAfterHiddenTab(false);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1148544")
    public void closeButton_navigatesToLandingPage() throws TimeoutException {
        Context context = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();
        Intent intent = CustomTabsTestUtils
                .createMinimalCustomTabIntent(context, mTestServer.getURL(TARGET_BLANK_TEST_PAGE));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        Assert.assertEquals(activity.getCurrentTabModel().getCount(), 1);

        // The link we click will take us to another page. Set the initial page as the landing page.
        activity.getComponent().resolveNavigationController().setLandingPageOnCloseCriterion(
                url -> url.contains(TARGET_BLANK_TEST_PAGE));

        DOMUtils.clickNode(activity.getActivityTab().getWebContents(), "target_blank_link");
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.getCurrentTabModel().getCount(), is(2)));

        ClickUtils.clickButton(activity.findViewById(R.id.close_button));

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.getCurrentTabModel().getCount(), is(1)));
        assertFalse(activity.isFinishing());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1148544")
    public void closeButton_closesActivityIfNoLandingPage() throws TimeoutException {
        Context context = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();
        Intent intent = CustomTabsTestUtils
                .createMinimalCustomTabIntent(context, mTestServer.getURL(TARGET_BLANK_TEST_PAGE));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
        Assert.assertEquals(activity.getCurrentTabModel().getCount(), 1);

        DOMUtils.clickNode(activity.getActivityTab().getWebContents(), "target_blank_link");
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.getCurrentTabModel().getCount(), is(2)));

        ClickUtils.clickButton(activity.findViewById(R.id.close_button));

        CriteriaHelper.pollUiThread(activity::isFinishing);
    }

    // The flags are necessary to ensure the experiment id 101 is honored.
    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=ExternalExperimentAllowlist",
            "force-fieldtrials=Trial/Group", "force-fieldtrial-params=Trial.Group:101/x"})
    public void
    testExperimentIds() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        intent.setData(Uri.parse(mTestPage));
        int[] ids = {101};
        intent.putExtra(CustomTabIntentDataProvider.EXPERIMENT_IDS, ids);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(
                token, "com.google.android.googlequicksearchbox");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { assertTrue(CustomTabsTestUtils.hasVariationId(101)); });
    }

    // The flags are necessary to ensure the experiment id 101 is honored.
    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=ExternalExperimentAllowlist",
            "force-fieldtrials=Trial/Group", "force-fieldtrial-params=Trial.Group:101/x"})
    public void
    testExperimentIdsFromMayLaunchUrl() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        intent.setData(Uri.parse(mTestPage));
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(
                token, "com.google.android.googlequicksearchbox");
        Bundle extrasBundle = new Bundle();
        int[] ids = {101};
        extrasBundle.putIntArray(CustomTabIntentDataProvider.EXPERIMENT_IDS, ids);
        Assert.assertTrue(connection.mayLaunchUrl(
                token, Uri.parse("https://www.google.com"), extrasBundle, null));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { assertTrue(CustomTabsTestUtils.hasVariationId(101)); });
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
            "ignore-certificate-errors", "ignore-google-port-numbers"})
    @DisabledTest(message = "https://crbug.com/1238931")
    public void
    testMayLaunchUrlAddsClientDataHeader() throws Exception {
        TestWebServer webServer = createTestWebServer();
        webServer.setServerHost("www.google.com");
        final String expectedHeader = "test-header";
        String url = webServer.setResponse("/ok.html", "<html>ok</html>", null);
        AppHooks.setInstanceForTesting(new AppHooksImpl() {
            @Override
            public CustomTabsConnection createCustomTabsConnection() {
                return new CustomTabsConnection() {
                    @Override
                    public void setClientDataHeaderForNewTab(
                            CustomTabsSessionToken session, WebContents webContents) {
                        setClientDataHeader(webContents, expectedHeader);
                    }
                };
            }
        });
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, url);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
        CriteriaHelper.pollUiThread(
                () -> { Criteria.checkThat(connection.getHiddenTab(), Matchers.notNullValue()); });
        Tab hiddenTab =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return connection.getHiddenTab(); });
        ChromeTabUtils.waitForTabPageLoaded(hiddenTab, url);
        String actualHeader = webServer.getLastRequest("/ok.html").headerValue("X-CCT-Client-Data");
        assertEquals(expectedHeader, actualHeader);
        webServer.shutdown();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
    public void testLaunchPartialCustomTabActivity() throws Exception {
        Intent intent = createMinimalCustomTabIntent();
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_INITIAL_ACTIVITY_HEIGHT_IN_PIXEL, 50);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        WindowManager.LayoutParams attributes = getActivity().getWindow().getAttributes();
        assertNotEquals("The height shouldn't be match_parent",
                WindowManager.LayoutParams.MATCH_PARENT, attributes.height);
    }

    private void verifyHistoryAfterHiddenTab(boolean speculationWasAHit) throws Exception {
        String speculationUrl = mTestPage;
        String navigationUrl = speculationWasAHit ? mTestPage : mTestPage2;
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(context, navigationUrl);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
        setCanUseHiddenTabForSession(connection, token, true);

        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(speculationUrl), null, null));
        CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, speculationUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = getActivity().getActivityTab();
        Assert.assertFalse(tab.canGoBack());
        Assert.assertFalse(tab.canGoForward());

        List<HistoryItem> history = getHistory(tab);
        assertEquals(1, history.size());
        assertEquals(navigationUrl, history.get(0).getUrl().getSpec());
    }

    private void mayLaunchUrlWithoutWarmup(boolean useHiddenTab) {
        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        Bundle extras = null;
        setCanUseHiddenTabForSession(connection, token, useHiddenTab);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(mTestPage), extras, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, mTestPage));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        assertEquals(mTestPage, ChromeTabUtils.getUrlStringOnUiThread(tab));
    }

    private ChromeActivity reparentAndVerifyTab() {
        final ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                ChromeTabbedActivity.class.getName(), /* result = */ null, false);
        final Tab tabToBeReparented = getActivity().getActivityTab();
        final CallbackHelper tabHiddenHelper = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                tabHiddenHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> tabToBeReparented.addObserver(observer));
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            getActivity().getComponent().resolveNavigationController()
                    .openCurrentUrlInBrowser(true);
            assertNull(getActivity().getActivityTab());
        });
        // Use the extended CriteriaHelper timeout to make sure we get an activity
        final Activity lastActivity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(
                "Monitor did not get an activity before hitting the timeout", lastActivity);
        Assert.assertTrue("Expected lastActivity to be a ChromeActivity, was "
                        + lastActivity.getClass().getName(),
                lastActivity instanceof ChromeActivity);
        final ChromeActivity newActivity = (ChromeActivity) lastActivity;
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(newActivity.getActivityTab(), Matchers.notNullValue());
            Criteria.checkThat(newActivity.getActivityTab(), is(tabToBeReparented));
        });
        assertEquals(newActivity.getWindowAndroid(), tabToBeReparented.getWindowAndroid());
        assertEquals(newActivity.getWindowAndroid(),
                tabToBeReparented.getWebContents().getTopLevelNativeWindow());
        Assert.assertFalse(TabTestUtils.getDelegateFactory(tabToBeReparented)
                                   instanceof CustomTabDelegateFactory);
        assertEquals("The tab should never be hidden during the reparenting process", 0,
                tabHiddenHelper.getCallCount());
        Assert.assertFalse(TabTestUtils.isCustomTab(tabToBeReparented));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tabToBeReparented.removeObserver(observer);
            RewindableIterator<TabObserver> observers =
                    TabTestUtils.getTabObservers(tabToBeReparented);
            while (observers.hasNext()) {
                Assert.assertFalse(observers.next() instanceof CustomTabObserver);
            }
        });
        return newActivity;
    }

    private void checkPageLoadMetrics(boolean allowMetrics) throws TimeoutException {
        CustomTabsExtraCallbackHelper<Long> firstContentfulPaintCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Long> largestContentfulPaintCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Long> loadEventStartCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Float> layoutShiftScoreCallback =
                new CustomTabsExtraCallbackHelper<>();
        CustomTabsExtraCallbackHelper<Boolean> sawNetworkQualityEstimatesCallback =
                new CustomTabsExtraCallbackHelper<>();

        final AtomicReference<Long> activityStartTimeMs = new AtomicReference<>(-1L);

        CustomTabsCallback cb = new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                if (callbackName.equals(CustomTabsConnection.ON_WARMUP_COMPLETED)) return;

                // Check if the callback name is either the Bottom Bar Scroll, or Page Load Metrics.
                // See https://crbug.com/963538 for why it might be either.
                if (!CustomTabsConnection.BOTTOM_BAR_SCROLL_STATE_CALLBACK.equals(callbackName)) {
                    assertEquals(CustomTabsConnection.PAGE_LOAD_METRICS_CALLBACK, callbackName);
                }
                if (-1 != args.getLong(PageLoadMetrics.EFFECTIVE_CONNECTION_TYPE, -1)) {
                    sawNetworkQualityEstimatesCallback.notifyCalled(true);
                }

                float layoutShiftScoreValue =
                        args.getFloat(PageLoadMetrics.LAYOUT_SHIFT_SCORE, -1f);
                if (layoutShiftScoreValue >= 0f) {
                    layoutShiftScoreCallback.notifyCalled(layoutShiftScoreValue);
                }

                long navigationStart = args.getLong(PageLoadMetrics.NAVIGATION_START, -1);
                if (navigationStart == -1) {
                    // Untested metric callback.
                    return;
                }
                long current = SystemClock.uptimeMillis();
                Assert.assertTrue(navigationStart <= current);
                Assert.assertTrue(navigationStart >= activityStartTimeMs.get());

                long firstContentfulPaint =
                        args.getLong(PageLoadMetrics.FIRST_CONTENTFUL_PAINT, -1);
                if (firstContentfulPaint > 0) {
                    Assert.assertTrue(firstContentfulPaint <= (current - navigationStart));
                    firstContentfulPaintCallback.notifyCalled(firstContentfulPaint);
                }

                long largestContentfulPaint =
                        args.getLong(PageLoadMetrics.LARGEST_CONTENTFUL_PAINT, -1);
                if (largestContentfulPaint > 0) {
                    Assert.assertTrue(largestContentfulPaint <= (current - navigationStart));
                    largestContentfulPaintCallback.notifyCalled(largestContentfulPaint);
                }

                long loadEventStart = args.getLong(PageLoadMetrics.LOAD_EVENT_START, -1);
                if (loadEventStart > 0) {
                    Assert.assertTrue(loadEventStart <= (current - navigationStart));
                    loadEventStartCallback.notifyCalled(loadEventStart);
                }
            }
        };

        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(cb).session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;

        if (allowMetrics) {
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
            connection.mClientManager.setShouldGetPageLoadMetricsForSession(token, true);
        }

        intent.setData(Uri.parse(mTestPage));
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        activityStartTimeMs.set(SystemClock.uptimeMillis());
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        if (allowMetrics) {
            firstContentfulPaintCallback.waitForCallback(0);
            loadEventStartCallback.waitForCallback(0);
            sawNetworkQualityEstimatesCallback.waitForCallback(0);

            assertTrue(firstContentfulPaintCallback.getValue() > 0);
            assertTrue(firstContentfulPaintCallback.getValue() > 0);
            assertTrue(sawNetworkQualityEstimatesCallback.getValue());
        } else {
            try {
                firstContentfulPaintCallback.waitForCallback(0);
                assertTrue(firstContentfulPaintCallback.getValue() > 0);
            } catch (TimeoutException e) {
                // Expected.
            }
            assertEquals(0, firstContentfulPaintCallback.getCallCount());

            try {
                largestContentfulPaintCallback.waitForCallback(0);
                assertTrue(largestContentfulPaintCallback.getValue() > 0);
            } catch (TimeoutException e) {
                // Expected.
            }

            assertEquals(0, largestContentfulPaintCallback.getCallCount());
        }

        // Navigate to a new page, as metrics like LCP are only reported at the end of the page load
        // lifetime.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
            activity.getComponent().resolveNavigationController().navigate("about:blank");
        });

        if (allowMetrics) {
            largestContentfulPaintCallback.waitForCallback(0, 1, 15, TimeUnit.SECONDS);
            assertTrue((long) largestContentfulPaintCallback.getValue() > 0);

            layoutShiftScoreCallback.waitForCallback(0);
            assertNotNull(layoutShiftScoreCallback.getValue());
        }
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession(Intent intentWithSession)
            throws Exception {
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intentWithSession);
        connection.newSession(token);
        intentWithSession.setData(Uri.parse(mTestPage));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intentWithSession);
        return token;
    }

    private CustomTabsSessionToken warmUpAndLaunchUrlWithSession() throws Exception {
        return warmUpAndLaunchUrlWithSession(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage));
    }

    private static class ElementContentCriteria implements Runnable {
        private final Tab mTab;
        private final String mJsFunction;
        private final String mExpected;

        public ElementContentCriteria(Tab tab, String elementId, String expected) {
            mTab = tab;
            mExpected = "\"" + expected + "\"";
            mJsFunction = "(function () { return document.getElementById(\"" + elementId
                    + "\").innerHTML; })()";
        }

        @Override
        public void run() {
            String value = null;
            try {
                String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTab.getWebContents(), mJsFunction);
                if (jsonText.equalsIgnoreCase("null")) jsonText = "";
                value = jsonText;
            } catch (TimeoutException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
            Criteria.checkThat("Page element is not as expected.", value, is(mExpected));
        }
    }

    private static List<HistoryItem> getHistory(Tab tab) throws TimeoutException {
        final TestBrowsingHistoryObserver historyObserver = new TestBrowsingHistoryObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.fromWebContents(tab.getWebContents());
            BrowsingHistoryBridge historyService = new BrowsingHistoryBridge(profile);
            historyService.setObserver(historyObserver);
            String historyQueryFilter = "";
            historyService.queryHistory(historyQueryFilter);
        });
        historyObserver.getQueryCallback().waitForCallback(0);
        return historyObserver.getHistoryQueryResults();
    }

    private SessionDataHolder getSessionDataHolder() {
        return ChromeApplicationImpl.getComponent().resolveSessionDataHolder();
    }
}
