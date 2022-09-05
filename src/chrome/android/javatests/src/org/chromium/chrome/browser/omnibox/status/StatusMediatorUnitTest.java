// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import androidx.test.filters.SmallTest;

import com.google.android.material.color.MaterialColors;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for {@link StatusMediator}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_PROMO_EXISTING_DEVICE)
public final class StatusMediatorUnitTest {
    private static final String TAG = "StatusMediatorUnitTest";
    private static final String TEST_SEARCH_URL = "https://www.test.com";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    NewTabPageDelegate mNewTabPageDelegate;
    @Mock
    LocationBarDataProvider mLocationBarDataProvider;
    @Mock
    UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    @Mock
    SearchEngineLogoUtils mSearchEngineLogoUtils;
    @Mock
    Profile mProfile;
    @Mock
    LibraryLoader mLibraryLoader;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    PermissionDialogController mPermissionDialogController;
    @Mock
    PageInfoIPHController mPageInfoIPHController;
    @Mock
    MerchantTrustSignalsCoordinator mMerchantTrustSignalsCoordinator;
    @Mock
    Drawable mStoreIconDrawable;

    Context mContext;
    Resources mResources;

    PropertyModel mModel;
    StatusMediator mMediator;
    Bitmap mBitmap;
    OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier;
    WindowAndroid mWindowAndroid;
    LibraryLoader mOriginalLibraryLoader;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mContext = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();
        mWindowAndroid =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> new WindowAndroid(mContext));

        mModel = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> new PropertyModel(StatusProperties.ALL_KEYS));

        mOriginalLibraryLoader = LibraryLoader.getInstance();
        doReturn(true).when(mLibraryLoader).isInitialized();
        LibraryLoader.setLibraryLoaderForTesting(mLibraryLoader);

        // By default return google g, but this behavior is overridden in some tests.
        Answer logoAnswer = (invocation) -> {
            ((Callback<StatusIconResource>) invocation.getArgument(4))
                    .onResult(new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
            return null;
        };
        doReturn(false).when(mLocationBarDataProvider).isInOverviewAndShowingOmnibox();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        doAnswer(logoAnswer)
                .when(mSearchEngineLogoUtils)
                .getSearchEngineLogo(
                        eq(mResources), eq(BrandedColorScheme.APP_DEFAULT), any(), any(), any());

        mBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        setupStatusMediator(/* isTablet= */ false);
    }

    @After
    public void tearDown() {
        LibraryLoader.setLibraryLoaderForTesting(mOriginalLibraryLoader);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mWindowAndroid.destroy(); });
    }

    private void setupStatusMediator(boolean isTablet) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTemplateUrlServiceSupplier = new OneshotSupplierImpl<>();
            ObservableSupplierImpl<MerchantTrustSignalsCoordinator>
                    merchantTrustSignalsCoordinatorObservableSupplier =
                            new ObservableSupplierImpl<>();
            mMediator = new StatusMediator(mModel, mResources, mContext,
                    mUrlBarEditingTextStateProvider, isTablet, mLocationBarDataProvider,
                    mPermissionDialogController, mSearchEngineLogoUtils,
                    mTemplateUrlServiceSupplier,
                    ()
                            -> mProfile,
                    mPageInfoIPHController, mWindowAndroid,
                    merchantTrustSignalsCoordinatorObservableSupplier);
            mTemplateUrlServiceSupplier.set(mTemplateUrlService);
            merchantTrustSignalsCoordinatorObservableSupplier.set(mMerchantTrustSignalsCoordinator);
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_hideAfterUnfocusFinished() {
        doReturn(UrlConstants.NTP_URL).when(mLocationBarDataProvider).getCurrentUrl();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(true);
        mMediator.setUrlHasFocus(false);
        Assert.assertFalse(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_noHideIconAfterUnfocusedWhenScrolled() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlFocusChangePercent(1f);
        mMediator.setUrlHasFocus(true);
        mMediator.setUrlHasFocus(false);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogoOnNtp() {
        doReturn(UrlConstants.NTP_URL).when(mLocationBarDataProvider).getCurrentUrl();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ false, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertFalse(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogoOnNtpTablet() {
        setupStatusMediator(/* isTablet= */ true);
        doReturn(UrlConstants.NTP_URL).when(mLocationBarDataProvider).getCurrentUrl();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ false, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertFalse(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_isGoogleLogo_whenScrolled() {
        doReturn(false).when(mLocationBarDataProvider).isLoading();
        doReturn(UrlConstants.NTP_URL).when(mLocationBarDataProvider).getCurrentUrl();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();
        doReturn(UrlConstants.NTP_URL).when(mLocationBarDataProvider).getCurrentUrl();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlFocusChangePercent(1f);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest

    @UiThreadTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest

    @UiThreadTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenAutocompleteSiteContainsText() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextDoesNotMatch() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest

    @UiThreadTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextIsEmpty() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        // Setup a valid url to prevent the default "" from matching the url.
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_incognitoStateChanged() {
        mMediator.onIncognitoStateChanged();

        Assert.assertEquals(false, mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertEquals(1f, mModel.get(StatusProperties.STATUS_ICON_ALPHA), 0f);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_incognitoNoIcon() {
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setSecurityIconResource(0);

        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconChanges() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setSecurityIconResource(0);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        Assert.assertTrue(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest

    @UiThreadTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconNoChanges() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(false);
        mMediator.setSecurityIconResource(0);
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        Assert.assertFalse(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextEmpty() {
        Assert.assertEquals("Empty urlBarText should resolve to empty urlBarTextWithAutocomplete",
                "", mMediator.resolveUrlBarTextWithAutocomplete(""));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextMismatchesAutocompleteText() {
        doReturn("https://foo.com").when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        String msg = "The urlBarText should only resolve to the autocomplete text if it's a "
                + "substring of the autocomplete text.";
        Assert.assertEquals(
                msg, "https://foo.com", mMediator.resolveUrlBarTextWithAutocomplete("foo.com"));
        Assert.assertEquals(msg, "bar.com", mMediator.resolveUrlBarTextWithAutocomplete("bar.com"));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testIncognitoStateChange_goingToIncognito() {
        mMediator.setShowIconsWhenUrlFocused(true);

        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        Assert.assertEquals(1f, mModel.get(StatusProperties.STATUS_ICON_ALPHA), 0f);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testIncognitoStateChange_backFromIncognito() {
        mMediator.setShowIconsWhenUrlFocused(true);

        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        Assert.assertEquals(1f, mModel.get(StatusProperties.STATUS_ICON_ALPHA), 0f);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testStatusText() {
        mMediator.setUnfocusedLocationBarWidth(10);
        mMediator.setPageIsOffline(true);
        mMediator.setPageIsPaintPreview(true);
        // When both states, offline, and preview are enabled, paint preview has
        // the highest priority.
        Assert.assertEquals("Incorrect text for paint preview status text",
                R.string.location_bar_paint_preview_page_status,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for paint preview status text",
                MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Assert.assertEquals("Incorrect color for paint preview status text",
                mContext.getColor(R.color.locationbar_status_preview_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));

        // When only offline is enabled, it should be shown.
        mMediator.setPageIsPaintPreview(false);
        mMediator.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        Assert.assertEquals("Incorrect text for offline page status text",
                R.string.location_bar_verbose_status_offline,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_light),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Assert.assertEquals("Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTemplateUrlServiceChanged() {
        setupSearchEngineLogoForTesting(
                /* showLogo= */ true, /* isGoogle= */ true, /* loupeEverywhere= */ false);

        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlHasFocus(true);

        mMediator.onTemplateURLServiceChanged();
        verify(mSearchEngineLogoUtils, times(3))
                .getSearchEngineLogo(
                        eq(mResources), eq(BrandedColorScheme.APP_DEFAULT), any(), any(), any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSetStoreIconController() {
        mMediator.setStoreIconController();
        verify(mMerchantTrustSignalsCoordinator, times(1)).setOmniboxIconController(eq(mMediator));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShowStoreIcon_DifferentUrl() {
        setupStoreIconForTesting("test1.com", false);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(mWindowAndroid, "test2.com", mStoreIconDrawable, 0, true);
        Assert.assertFalse(mMediator.isStoreIconShowing());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShowStoreIcon_InIncognito() {
        setupStoreIconForTesting("test.com", true);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(mWindowAndroid, "test.com", mStoreIconDrawable, 0, true);
        Assert.assertFalse(mMediator.isStoreIconShowing());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShowStoreIcon() {
        setupStoreIconForTesting("test.com", false);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(mWindowAndroid, "test.com", mStoreIconDrawable, 0, true);
        Assert.assertTrue(mMediator.isStoreIconShowing());
        Assert.assertEquals(IconTransitionType.ROTATE,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getTransitionType());
        Assert.assertNotNull(
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(1)).showStoreIconIPH(anyInt(), eq(0));

        // Simulate that the store icon is blown away by other customized icon.
        mMediator.resetCustomIconsStatus();
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Show store icon again.
        mMediator.showStoreIcon(mWindowAndroid, "test.com", mStoreIconDrawable, 0, true);
        Assert.assertTrue(mMediator.isStoreIconShowing());

        // Simulate that we need to switch back to the default icon.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShowStoreIcon_NotEligibleToShowIph() {
        setupStoreIconForTesting("test.com", false);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(mWindowAndroid, "test.com", mStoreIconDrawable, 0, false);
        Assert.assertTrue(mMediator.isStoreIconShowing());
        Assert.assertEquals(IconTransitionType.ROTATE,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getTransitionType());
        Assert.assertNotNull(
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(0)).showStoreIconIPH(anyInt(), eq(0));
    }

    /**
     * @param showLogo Whether the search engine logo should be shown.
     * @param isGoogle Whether the search engine is Google.
     * @param loupeEverywhere Whether to show the loupe everywhere.
     */
    private void setupSearchEngineLogoForTesting(
            boolean showLogo, boolean isGoogle, boolean loupeEverywhere) {
        doReturn(showLogo).when(mSearchEngineLogoUtils).shouldShowSearchEngineLogo(false);
        doReturn(false).when(mSearchEngineLogoUtils).shouldShowSearchEngineLogo(true);
    }

    /**
     * @param currentUrl Url of current page.
     * @param isIncognito Whether the current page is in an incognito mode.
     */
    private void setupStoreIconForTesting(String currentUrl, boolean isIncognito) {
        doReturn(currentUrl).when(mLocationBarDataProvider).getCurrentUrl();
        doReturn(isIncognito).when(mLocationBarDataProvider).isIncognito();
    }
}
