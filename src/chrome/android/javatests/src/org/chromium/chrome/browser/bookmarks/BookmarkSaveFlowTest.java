// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkType;
import org.chromium.chrome.browser.power_bookmarks.ShoppingSpecifics;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Tests for the bookmark save flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BookmarkSaveFlowTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public DisableAnimationsTestRule mDisableAnimationsTestRule = new DisableAnimationsTestRule();
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    SubscriptionsManager mSubscriptionsManager;

    @Mock
    private UserEducationHelper mUserEducationHelper;

    private BookmarkSaveFlowCoordinator mBookmarkSaveFlowCoordinator;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;
    private BookmarkModel mBookmarkModel;
    private BookmarkBridge mBookmarkBridge;

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeActivityTestRule.waitForActivityNativeInitializationComplete(
                mActivityTestRule.getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mBottomSheetController =
                    cta.getRootUiCoordinatorForTesting().getBottomSheetController();
            mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
            mBookmarkSaveFlowCoordinator = new BookmarkSaveFlowCoordinator(
                    cta, mBottomSheetController, mSubscriptionsManager, mUserEducationHelper);
            mBookmarkModel = new BookmarkModel(Profile.fromWebContents(
                    mActivityTestRule.getActivity().getActivityTab().getWebContents()));
            mBookmarkBridge = mActivityTestRule.getActivity().getBookmarkBridgeForTesting();
        });

        loadBookmarkModel();
        doAnswer((invocation) -> {
            ((Callback<Integer>) invocation.getArgument(1))
                    .onResult(SubscriptionsManager.StatusCode.OK);
            return null;
        })
                .when(mSubscriptionsManager)
                .subscribe(any(CommerceSubscription.class), any());
        doAnswer((invocation) -> {
            ((Callback<Integer>) invocation.getArgument(1))
                    .onResult(SubscriptionsManager.StatusCode.OK);
            return null;
        })
                .when(mSubscriptionsManager)
                .unsubscribe(any(CommerceSubscription.class), any());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(id);
            return null;
        });
        mRenderTestRule.render(
                mBookmarkSaveFlowCoordinator.getViewForTesting(), "bookmark_save_flow");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_BookmarkMoved() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(
                    id, /*fromExplicitTrackUi=*/false, /*wasBookmarkMoved=*/true);
            return null;
        });
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_bookmark_moved");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_WithShoppingListItem() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            PowerBookmarkMeta.Builder meta =
                    PowerBookmarkMeta.newBuilder()
                            .setType(PowerBookmarkType.SHOPPING)
                            .setShoppingSpecifics(ShoppingSpecifics.newBuilder()
                                                          .setProductClusterId(1234L)
                                                          .build());
            mBookmarkBridge.setPowerBookmarkMeta(id, meta.build());
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/false,
                    /*wasBookmarkMoved=*/false, meta.build());
            return null;
        });
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_shopping_list_item");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_WithShoppingListItem_fromHeuristicEntryPoint()
            throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            PowerBookmarkMeta.Builder meta =
                    PowerBookmarkMeta.newBuilder()
                            .setType(PowerBookmarkType.SHOPPING)
                            .setShoppingSpecifics(ShoppingSpecifics.newBuilder()
                                                          .setProductClusterId(1234L)
                                                          .build());
            mBookmarkBridge.setPowerBookmarkMeta(id, meta.build());
            mBookmarkSaveFlowCoordinator.show(
                    id, /*fromHeuristicEntryPoint=*/true, /*wasBookmarkMoved=*/false, meta.build());
            return null;
        });
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_shopping_list_item_from_heuristic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarkSaveFlow_WithShoppingListItem_fromHeuristicEntryPoint_saveFailed()
            throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            PowerBookmarkMeta.Builder meta =
                    PowerBookmarkMeta.newBuilder()
                            .setType(PowerBookmarkType.SHOPPING)
                            .setShoppingSpecifics(
                                    ShoppingSpecifics.newBuilder().setProductClusterId(1234L));
            mBookmarkBridge.setPowerBookmarkMeta(id, meta.build());
            mBookmarkSaveFlowCoordinator.show(id, /*fromHeuristicEntryPoint=*/false,
                    /*wasBookmarkMoved=*/false, meta.build());
            return null;
        });
        doAnswer((invocation) -> {
            ((Callback<Integer>) invocation.getArgument(1))
                    .onResult(SubscriptionsManager.StatusCode.NETWORK_ERROR);
            return null;
        })
                .when(mSubscriptionsManager)
                .subscribe(any(CommerceSubscription.class), any());
        onView(withId(R.id.notification_switch)).perform(click());
        mRenderTestRule.render(mBookmarkSaveFlowCoordinator.getViewForTesting(),
                "bookmark_save_flow_shopping_list_item_from_heuristic_save_failed");
    }

    @Test
    @MediumTest
    public void testBookmarkSaveFlowEdit() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(
                    id, /*fromHeuristicEntryPoint=*/false, /*wasBookmarkMoved=*/false);
            return null;
        });
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ClickUtils.clickButton(cta.findViewById(R.id.bookmark_edit));
        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.edit_bookmark)))
                .check(matches(isDisplayed()));

        // Dismiss the activity.
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    public void testBookmarkSaveFlowChooseFolder() throws IOException {
        TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            BookmarkId id = addBookmark("Test bookmark", new GURL("http://a.com"));
            mBookmarkSaveFlowCoordinator.show(
                    id, /*fromHeuristicEntryPoint=*/false, /*wasBookmarkMoved=*/false);
            return null;
        });
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ClickUtils.clickButton(cta.findViewById(R.id.bookmark_select_folder));
        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.bookmark_choose_folder)))
                .check(matches(isDisplayed()));

        // Dismiss the activity.
        Espresso.pressBack();
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.BOOKMARKS_IMPROVED_SAVE_FLOW + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:" + BookmarkFeatures.AUTODISMISS_ENABLED_PARAM_NAME
                    + "/true/" + BookmarkFeatures.AUTODISMISS_LENGTH_PARAM_NAME + "/0"})
    public void
    testBookmarkSaveFlowAutodismiss() {
        // The test parameters above will make the save flow autodismiss immediately.
        onView(withId(R.id.title_text)).check(doesNotExist());
    }

    private void loadBookmarkModel() {
        // Do not read partner bookmarks in setUp(), so that the lazy reading is covered.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PartnerBookmarksShim.kickOffReading(mActivityTestRule.getActivity()));
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    private BookmarkId addBookmark(final String title, final GURL url) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }
}