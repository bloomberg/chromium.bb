// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/**
 * Tests {@link WebFeedMainMenuItem}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
@SmallTest
public final class WebFeedMainMenuItemTest {
    private static final GURL TEST_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
    private static final GURL FAVICON_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private Activity mActivity;
    @Mock
    private FeedLauncher mFeedLauncher;
    @Mock
    private AppMenuHandler mAppMenuHandler;
    @Mock
    private ModalDialogManager mDialogManager;
    @Mock
    private SnackbarManager mSnackBarManager;
    @Mock
    private Tab mTab;
    @Mock
    public WebFeedBridge.Natives mWebFeedBridgeJniMock;
    @Mock
    public UrlFormatter.Natives mUrlFormatterJniMock;
    private TestWebFeedFaviconFetcher mFaviconFetcher = new TestWebFeedFaviconFetcher();

    private WebFeedMainMenuItem mWebFeedMainMenuItem;
    private ArrayList<Callback<WebFeedBridge.WebFeedMetadata>> mWaitingMetadataCallbacks =
            new ArrayList();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Print logs to stdout.
        ShadowLog.stream = System.out;
        mJniMocker.mock(WebFeedBridge.getTestHooksForTesting(), mWebFeedBridgeJniMock);

        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        doAnswer(invocation -> { return invocation.<GURL>getArgument(0).getHost(); })
                .when(mUrlFormatterJniMock)
                .formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(any());

        doReturn(GURL.emptyGURL()).when(mTab).getOriginalUrl();
        doReturn(false).when(mTab).isShowingErrorPage();

        mActivity = Robolectric.setupActivity(Activity.class);
        // Required for resolving an attribute used in AppMenuItemText.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        // Add requests for web feed information to mWaitingMetadataCallbacks.
        doAnswer(invocation -> {
            mWaitingMetadataCallbacks.add(
                    invocation.<Callback<WebFeedBridge.WebFeedMetadata>>getArgument(1));
            return null;
        })
                .when(mWebFeedBridgeJniMock)
                .findWebFeedInfoForPage(any(), any(Callback.class));

        mWebFeedMainMenuItem = (WebFeedMainMenuItem) (LayoutInflater.from(mActivity).inflate(
                R.layout.web_feed_main_menu_item, null));

        LoadingView.setDisableAnimationForTest(true);
    }

    @After
    public void tearDown() {
        LoadingView.setDisableAnimationForTest(false);
    }

    @Test
    @UiThreadTest
    public void initialize_hasFavicon_displaysFavicon() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(null);
        mFaviconFetcher.answerWithBitmap();

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        Bitmap actualIcon = ((BitmapDrawable) imageView.getDrawable()).getBitmap();
        assertEquals("Icon should be favicon.", mFaviconFetcher.getTestBitmap(), actualIcon);
        assertEquals("Icon should be visible.", View.VISIBLE, imageView.getVisibility());
    }

    @Test
    @UiThreadTest
    public void initialize_emptyUrl_removesIcon() {
        doReturn(GURL.emptyGURL()).when(mTab).getOriginalUrl();
        mWebFeedMainMenuItem.initialize(mTab, mAppMenuHandler, mFaviconFetcher, mFeedLauncher,
                mDialogManager, mSnackBarManager);
        respondWithFeedMetadata(null);
        mFaviconFetcher.answerWithNull();

        ImageView imageView = mWebFeedMainMenuItem.findViewById(R.id.icon);
        assertEquals("Icon should be gone.", View.GONE, imageView.getVisibility());
    }

    @Test
    @UiThreadTest
    public void initialize_displaysCorrectTitle() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(null);

        TextView textView = mWebFeedMainMenuItem.findViewById(R.id.menu_item_text);
        assertEquals("Title should be shortened URL.",
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(TEST_URL),
                textView.getText());
    }

    @Test
    @UiThreadTest
    public void initialize_noMetadata_displaysFollowChip() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(null);

        assertEquals("follow", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_notFollowed_displaysFollowChip() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, GURL.emptyGURL()));

        assertEquals("follow", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_errorPage_displaysDisabledFollowChip() {
        doReturn(true).when(mTab).isShowingErrorPage();
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, GURL.emptyGURL()));

        assertEquals("disabled follow", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_unknownFollowStatus_displaysFollowChip() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.UNKNOWN, GURL.emptyGURL()));

        assertEquals("follow", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_followed_displaysFollowingChip() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBED, GURL.emptyGURL()));

        assertEquals("following", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_unfollowInProgress() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(createWebFeedMetadata(
                WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS, GURL.emptyGURL()));

        // ChipView imposes a delay.
        assertEquals("invisible", getChipState());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals("disabled following", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_unfollowInProgress_succeeds() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(createWebFeedMetadata(
                WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS, GURL.emptyGURL()));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, GURL.emptyGURL()));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals("follow", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_unfollowInProgress_fails() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(createWebFeedMetadata(
                WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS, GURL.emptyGURL()));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBED, GURL.emptyGURL()));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals("following", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_followInProgress_succeeds() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(createWebFeedMetadata(
                WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS, GURL.emptyGURL()));

        // ChipView imposes a delay.
        assertEquals("invisible", getChipState());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals("disabled follow", getChipState());

        // Now the web feed is subscribed.
        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.SUBSCRIBED, GURL.emptyGURL()));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals("following", getChipState());
    }

    @Test
    @UiThreadTest
    public void initialize_followInProgress_fails() {
        initializeWebFeedMainMenuItem();
        respondWithFeedMetadata(createWebFeedMetadata(
                WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS, GURL.emptyGURL()));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        respondWithFeedMetadata(
                createWebFeedMetadata(WebFeedSubscriptionStatus.NOT_SUBSCRIBED, GURL.emptyGURL()));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals("follow", getChipState());
    }

    /** Helper method to return the visual state of the chip. */
    private String getChipState() {
        // Note there are some invariants checked here:
        // * There are two ChipViews, ensure that only one is non-GONE.
        // * For the visible chip, if any, verify it has the right text.
        ChipView followingChip = getFollowingChipView();
        ChipView followChip = getFollowChipView();

        if (followingChip.getVisibility() != View.GONE) {
            assertEquals(View.GONE, followChip.getVisibility());
            if (followingChip.getVisibility() == View.VISIBLE) {
                assertEquals(mActivity.getResources().getString(R.string.menu_following),
                        followingChip.getPrimaryTextView().getText());
                return (followingChip.isEnabled() ? "" : "disabled ") + "following";
            }
            return "invisible";
        } else if (followChip.getVisibility() != View.GONE) {
            if (followChip.getVisibility() == View.VISIBLE) {
                assertEquals(mActivity.getResources().getString(R.string.menu_follow),
                        followChip.getPrimaryTextView().getText());
                return (followChip.isEnabled() ? "" : "disabled ") + "follow";
            }
            return "invisible";
        } else {
            return "none";
        }
    }

    /**
     * Helper method to initialize {@code mWebFeedMainMenuItem} with standard parameters.
     */
    private void initializeWebFeedMainMenuItem() {
        doReturn(TEST_URL).when(mTab).getOriginalUrl();
        mWebFeedMainMenuItem.initialize(mTab, mAppMenuHandler, mFaviconFetcher, mFeedLauncher,
                mDialogManager, mSnackBarManager);
    }

    /**
     * Helper method to create a {@link WebFeedBridge.WebFeedMetadata} with standard parameters.
     *
     * @param subscriptionStatus {@link WebFeedSubscriptionStatus} for the metadata.
     */
    private WebFeedBridge.WebFeedMetadata createWebFeedMetadata(
            @WebFeedSubscriptionStatus int subscriptionStatus, GURL faviconUrl) {
        return new WebFeedBridge.WebFeedMetadata("id".getBytes(), "title", TEST_URL,
                subscriptionStatus, WebFeedAvailabilityStatus.INACTIVE, /*isRecommended=*/false,
                faviconUrl);
    }

    ChipView getFollowChipView() {
        return mWebFeedMainMenuItem.findViewById(R.id.follow_chip_view);
    }

    ChipView getFollowingChipView() {
        return mWebFeedMainMenuItem.findViewById(R.id.following_chip_view);
    }

    private void respondWithFeedMetadata(WebFeedBridge.WebFeedMetadata metadata) {
        assertFalse(mWaitingMetadataCallbacks.isEmpty());
        Callback<WebFeedBridge.WebFeedMetadata> callback = mWaitingMetadataCallbacks.get(0);
        mWaitingMetadataCallbacks.remove(0);
        callback.onResult(metadata);
    }
}
