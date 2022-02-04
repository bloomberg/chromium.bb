// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;

import android.app.Activity;
import android.os.Build;
import android.os.Looper;
import android.support.test.runner.lifecycle.Stage;
import android.view.View;

import androidx.test.filters.MediumTest;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests {@link ChromeProvidedSharingOptionsProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromeProvidedSharingOptionsProviderTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private static final String URL = "http://www.google.com/";

    @Mock
    private UserPrefs.Natives mUserPrefsNatives;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private ShareSheetCoordinator mShareSheetCoordinator;
    @Mock
    private Supplier<Tab> mTabProvider;
    @Mock
    private Tab mTab;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private WebContents mWebContents;
    @Mock
    private Tracker mTracker;
    @Mock
    private ShareParams.TargetChosenCallback mTargetChosenCallback;

    private Activity mActivity;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        Profile.setLastUsedProfileForTesting(mProfile);
        Mockito.when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        Mockito.when(mTabProvider.get()).thenReturn(mTab);
        Mockito.when(mTab.getWebContents()).thenReturn(mWebContents);
        Mockito.when(mTab.getUrl()).thenReturn(new GURL(URL));
        Mockito.when(mTab.isIncognito()).thenReturn(false);
        Mockito.doNothing().when(mBottomSheetController).hideContent(any(), anyBoolean());

        TrackerFactory.setTrackerForTests(mTracker);
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() throws Exception {
        TrackerFactory.setTrackerForTests(null);
        if (mActionTester != null) mActionTester.tearDown();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    @DisabledTest(message = "https://crbug.com/1233184")
    public void getPropertyModels_screenshotEnabled() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(
                        mActivity.getResources().getString(R.string.sharing_webnotes_create_card),
                        mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_long_screenshot),
                        mActivity.getResources().getString(R.string.sharing_copy_image),
                        mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                        mActivity.getResources().getString(R.string.sharing_save_image)));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "http://crbug/1285362")
    @Features.EnableFeatures({ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void getPropertyModels_lightweightReactionsEnabled() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(
                        mActivity.getResources().getString(R.string.sharing_webnotes_create_card),
                        mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_long_screenshot),
                        mActivity.getResources().getString(R.string.sharing_lightweight_reactions),
                        mActivity.getResources().getString(R.string.sharing_copy_image),
                        mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                        mActivity.getResources().getString(R.string.sharing_save_image)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    @DisabledTest(message = "https://crbug.com/1233184")
    public void getPropertyModels_printingEnabled_includesPrinting() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(
                        mActivity.getResources().getString(R.string.sharing_webnotes_create_card),
                        mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_long_screenshot),
                        mActivity.getResources().getString(R.string.sharing_copy_image),
                        mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                        mActivity.getResources().getString(R.string.print_share_activity_title),
                        mActivity.getResources().getString(R.string.sharing_save_image)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getPropertyModels_sharingHub15Enabled_includesCopyText() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT), DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_screenshot),
                        mActivity.getResources().getString(R.string.sharing_copy_text)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getPropertyModels_linkAndTextShare() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_AND_TEXT,
                                ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getPropertyModels_linkShare() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getPropertyModels_textShare() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.TEXT), DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_text)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT})
    @DisabledTest(message = "https://crbug.com/1233184")
    public void getPropertyModels_multiWindow_doesNotIncludeScreenshot() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.MAX);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES_FOR_TEST,
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/true);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(
                        mActivity.getResources().getString(R.string.sharing_webnotes_create_card),
                        mActivity.getResources().getString(R.string.sharing_copy_image),
                        mActivity.getResources().getString(R.string.sharing_copy),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                        mActivity.getResources().getString(R.string.sharing_save_image)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getPropertyModels_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_url),
                        mActivity.getResources().getString(
                                R.string.send_tab_to_self_share_activity_title),
                        mActivity.getResources().getString(R.string.qr_code_share_icon_label)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getPropertyModels_multipleTypes_filtersByContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE),
                        DetailedContentType.NOT_SPECIFIED,
                        /*isMultiWindow=*/false);

        // Long Screenshots is supported >= Android N (7.0).
        List<String> expectedModels = new ArrayList<String>();
        expectedModels.add(mActivity.getResources().getString(R.string.sharing_screenshot));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            expectedModels.add(
                    mActivity.getResources().getString(R.string.sharing_long_screenshot));
        }
        expectedModels.addAll(ImmutableList.of(
                mActivity.getResources().getString(R.string.sharing_lightweight_reactions),
                mActivity.getResources().getString(R.string.sharing_copy_url),
                mActivity.getResources().getString(R.string.sharing_copy_image),
                mActivity.getResources().getString(R.string.send_tab_to_self_share_activity_title),
                mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                mActivity.getResources().getString(R.string.sharing_save_image)));

        assertCorrectModelsAreInTheRightOrder(propertyModels, expectedModels);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void getPropertyModels_nonLightweightReactions_doesNotFilterByDetailedContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.IMAGE), DetailedContentType.IMAGE,
                        /*isMultiWindow=*/false);

        List<String> expectedModels = new ArrayList<String>();
        expectedModels.add(mActivity.getResources().getString(R.string.sharing_screenshot));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            expectedModels.add(
                    mActivity.getResources().getString(R.string.sharing_long_screenshot));
        }
        expectedModels.addAll(ImmutableList.of(
                mActivity.getResources().getString(R.string.sharing_copy_image),
                mActivity.getResources().getString(R.string.send_tab_to_self_share_activity_title),
                mActivity.getResources().getString(R.string.qr_code_share_icon_label),
                mActivity.getResources().getString(R.string.sharing_save_image)));

        assertCorrectModelsAreInTheRightOrder(propertyModels, expectedModels);
    }

    @Test
    @MediumTest
    public void getPropertyModels_webnotes_filtersByDetailedContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.IMAGE), DetailedContentType.WEB_NOTES,
                        /*isMultiWindow=*/false);

        List<String> expectedModels =
                ImmutableList.<String>builder()
                        .add(mActivity.getResources().getString(R.string.sharing_copy_image))
                        .add(mActivity.getResources().getString(R.string.sharing_save_image))
                        .build();

        assertCorrectModelsAreInTheRightOrder(propertyModels, expectedModels);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void getPropertyModels_lightweightReactions_filtersByDetailedContentType() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/true, LinkGeneration.MAX);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.IMAGE),
                        DetailedContentType.LIGHTWEIGHT_REACTION,
                        /*isMultiWindow=*/false);

        assertCorrectModelsAreInTheRightOrder(propertyModels,
                ImmutableList.of(mActivity.getResources().getString(R.string.sharing_copy_gif)));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION})
    @Features.DisableFeatures({ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT,
            ChromeFeatureList.LIGHTWEIGHT_REACTIONS})
    public void
    getShareDetailsMetrics_LinkGeneration() {
        @LinkGeneration
        int linkGenerationStatus = LinkGeneration.LINK;

        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, linkGenerationStatus);
        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.HIGHLIGHTED_TEXT),
                        DetailedContentType.NOT_SPECIFIED, /*isMultiWindow=*/false);

        assertCorrectLinkGenerationMetrics(propertyModels, linkGenerationStatus);
    }

    @Test
    @MediumTest
    public void getPropertyModels_onClick_callsOnTargetChosen() {
        setUpChromeProvidedSharingOptionsProviderTest(
                /*printingEnabled=*/false, LinkGeneration.LINK);

        List<PropertyModel> propertyModels =
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        ImmutableSet.of(ContentType.LINK_PAGE_VISIBLE),
                        DetailedContentType.NOT_SPECIFIED, /*isMultiWindow=*/false);
        View.OnClickListener onClickListener =
                propertyModels.get(0).get(ShareSheetItemViewProperties.CLICK_LISTENER);

        onClickListener.onClick(null);
        Mockito.verify(mTargetChosenCallback, Mockito.times(1))
                .onTargetChosen(ChromeProvidedSharingOptionsProvider
                                        .CHROME_PROVIDED_FEATURE_COMPONENT_NAME);
    }

    private void setUpChromeProvidedSharingOptionsProviderTest(
            boolean printingEnabled, @LinkGeneration int linkGenerationStatus) {
        Mockito.when(mPrefService.getBoolean(anyString())).thenReturn(printingEnabled);

        ShareParams shareParams = new ShareParams.Builder(null, /*title=*/"", /*url=*/"")
                                          .setCallback(mTargetChosenCallback)
                                          .setText("")
                                          .build();
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(mActivity,
                mTabProvider, mBottomSheetController,
                new ShareSheetBottomSheetContent(mActivity, null, mShareSheetCoordinator,
                        shareParams, /*featureEngagementTracker=*/null),
                shareParams,
                /*TabPrinterDelegate=*/null,
                /*settingsLauncher=*/null,
                /*syncState=*/false,
                /*shareStartTime=*/0, mShareSheetCoordinator,
                /*imageEditorModuleProvider*/ null, mTracker, URL, linkGenerationStatus,
                new LinkToggleMetricsDetails(
                        LinkToggleState.COUNT, DetailedContentType.NOT_SPECIFIED));
    }

    private void assertCorrectModelsAreInTheRightOrder(
            List<PropertyModel> propertyModels, List<String> expectedOrder) {
        ImmutableList.Builder<String> actualLabelOrder = ImmutableList.builder();
        for (PropertyModel propertyModel : propertyModels) {
            actualLabelOrder.add(propertyModel.get(ShareSheetItemViewProperties.LABEL));
        }
        assertEquals(
                "Property models in the wrong order.", expectedOrder, actualLabelOrder.build());
    }

    private void assertCorrectLinkGenerationMetrics(
            List<PropertyModel> propertyModels, @LinkGeneration int linkGenerationStatus) {
        Looper.prepare();
        mActionTester = new UserActionTester();
        View view = Mockito.mock(View.class);
        for (PropertyModel propertyModel : propertyModels) {
            // There is no link generation for Stylize Cards yet.
            if (propertyModel.get(ShareSheetItemViewProperties.LABEL)
                            .equals(mActivity.getResources().getString(
                                    R.string.sharing_webnotes_create_card))) {
                continue;
            }

            // There is no link generation for Screenshots yet either.
            if (propertyModel.get(ShareSheetItemViewProperties.LABEL)
                            .equals(mActivity.getResources().getString(
                                    R.string.sharing_screenshot))) {
                continue;
            }

            View.OnClickListener listener =
                    propertyModel.get(ShareSheetItemViewProperties.CLICK_LISTENER);
            listener.onClick(view);

            switch (linkGenerationStatus) {
                case LinkGeneration.LINK:
                    assertTrue(
                            "Expected a SharingHubAndroid...Success.LinkToTextShared user action",
                            mActionTester.getActions().contains(
                                    "SharingHubAndroid.LinkGeneration.Success.LinkToTextShared"));
                    assertEquals("Expected a 'link' shared stated metric to be a recorded", 1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "SharedHighlights.AndroidShareSheet.SharedState",
                                    LinkGeneration.LINK));
                    break;
                case LinkGeneration.TEXT:
                    assertTrue("Expected a SharingHubAndroid...Success.TextShared user action",
                            mActionTester.getActions().contains(
                                    "SharingHubAndroid.LinkGeneration.Success.TextShared"));
                    assertEquals("Expected a 'text' shared stated metric to be a recorded", 1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "SharedHighlights.AndroidShareSheet.SharedState",
                                    LinkGeneration.TEXT));
                    break;
                case LinkGeneration.FAILURE:
                    assertTrue("Expected a SharingHubAndroid...Failure.TextShared user action",
                            mActionTester.getActions().contains(
                                    "SharingHubAndroid.LinkGeneration.Failure.TextShared"));
                    assertEquals("Expected a 'failure' shared stated metric to be a recorded", 1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "SharedHighlights.AndroidShareSheet.SharedState",
                                    LinkGeneration.FAILURE));
                    break;
                default:
                    break;
            }
        }
    }
}
