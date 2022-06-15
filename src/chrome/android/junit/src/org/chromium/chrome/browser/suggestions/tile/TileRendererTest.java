// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** A simple test for {@link TileRenderer} using real {@link android.view.View} objects. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class, ShadowPostTask.class})
public class TileRendererTest {
    /**
     * Backend that substitutes normal PostTask operations. Allow us to coordinate task execution
     * without having to wait or yield.
     */
    private static class ShadowPostTaskImpl extends ShadowPostTask.TestImpl {
        private final List<Runnable> mRunnables = new ArrayList<>();

        @Override
        public void postDelayedTask(TaskTraits traits, Runnable task, long delay) {
            mRunnables.add(task);
        }

        void runAll() {
            for (int index = 0; index < mRunnables.size(); index++) {
                mRunnables.get(index).run();
            }
            mRunnables.clear();
        }
    }

    private static final int TITLE_LINES = 1;
    private static final GURL TEST_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    @Mock
    private ImageFetcher mMockImageFetcher;

    @Mock
    private TileGroup.TileSetupDelegate mTileSetupDelegate;

    @Mock
    private Runnable mTileSetupCallback;

    @Mock
    private TemplateUrlService mMockTemplateUrlService;

    @Mock
    private RoundedIconGenerator mIconGenerator;

    @Mock
    private Bitmap mBitmap;

    @Mock
    private ColorStateList mFakeColorStateList;

    private ShadowPostTaskImpl mPostTaskRunner;
    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private LinearLayout mSharedParent;
    private final ArgumentCaptor<Drawable> mIconCaptor = ArgumentCaptor.forClass(Drawable.class);
    private final ArgumentCaptor<LargeIconCallback> mImageFetcherCallbackCaptor =
            ArgumentCaptor.forClass(LargeIconCallback.class);

    private Tile mTile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivityController.setup();
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mPostTaskRunner = new ShadowPostTaskImpl();
        ShadowPostTask.setTestImpl(mPostTaskRunner);

        LibraryLoader.getInstance().setLibrariesLoadedForNativeTests();
        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);

        mSharedParent = new LinearLayout(mActivity);
        SiteSuggestion siteSuggestion =
                new SiteSuggestion("Example", TEST_URL, 0, TileSource.TOP_SITES, 0);
        mTile = new Tile(siteSuggestion, 0);
        mTile.setIconTint(mFakeColorStateList);

        // Set up mocks.
        doReturn(mTileSetupCallback).when(mTileSetupDelegate).createIconLoadCallback(any());
        doReturn(mBitmap).when(mIconGenerator).generateIconForUrl(any(GURL.class));
    }

    private void buildTileView(@TileStyle int style) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TileRenderer tileRenderer =
                    new TileRenderer(mActivity, style, TITLE_LINES, mMockImageFetcher);
            tileRenderer.setIconGeneratorForTesting(mIconGenerator);
            SuggestionsTileView tileView =
                    tileRenderer.buildTileView(mTile, mSharedParent, mTileSetupDelegate);
            Assert.assertNotNull(tileView);
        });
    }

    @Test
    @SmallTest
    public void testBuildTestView_Modern_noDecoration() {
        buildTileView(TileStyle.MODERN);
        // Expect no callbacks: we don't have any icon to offer there.
        mPostTaskRunner.runAll();
        verify(mTileSetupCallback, times(0)).run();
    }

    @Test
    @SmallTest
    public void testBuildTileView_ModernCondensed_noDecoration() {
        buildTileView(TileStyle.MODERN_CONDENSED);
        // Expect no callbacks: we don't have any icon to offer there.
        mPostTaskRunner.runAll();
        verify(mTileSetupCallback, times(0)).run();
    }

    @Test
    @SmallTest
    public void testBuildTileView_ModernCondensed_fallbackColor() {
        buildTileView(TileStyle.MODERN_CONDENSED);
        // Expect no callbacks: we don't have any icon to offer there.
        mPostTaskRunner.runAll();
        verify(mMockImageFetcher, times(1))
                .makeLargeIconRequest(any(), anyInt(), mImageFetcherCallbackCaptor.capture());
        verify(mTileSetupCallback, times(0)).run();
        mImageFetcherCallbackCaptor.getValue().onLargeIconAvailable(
                null, 0xace0ba5e, false, IconType.FAVICON);
        verify(mTileSetupCallback, times(1)).run();

        Assert.assertEquals(IconType.FAVICON, mTile.getIconType());
        Assert.assertEquals(TileVisualType.ICON_COLOR, mTile.getType());
        Assert.assertNull(mTile.getIconTint());

        BitmapDrawable drawable = (BitmapDrawable) mTile.getIcon();
        Assert.assertEquals(mBitmap, drawable.getBitmap());
    }

    @Test
    @SmallTest
    public void testBuildTileView_ModernCondensed_favicon() {
        buildTileView(TileStyle.MODERN_CONDENSED);
        // Expect no callbacks: we don't have any icon to offer there.
        mPostTaskRunner.runAll();
        verify(mMockImageFetcher, times(1))
                .makeLargeIconRequest(any(), anyInt(), mImageFetcherCallbackCaptor.capture());
        verify(mTileSetupCallback, times(0)).run();
        mImageFetcherCallbackCaptor.getValue().onLargeIconAvailable(
                mBitmap, 0xace0ba5e, false, IconType.TOUCH_ICON);
        verify(mTileSetupCallback, times(1)).run();

        Assert.assertEquals(IconType.TOUCH_ICON, mTile.getIconType());
        Assert.assertEquals(TileVisualType.ICON_REAL, mTile.getType());
        // Note: no way to test what bitmap got used here.
        Assert.assertNotNull(mTile.getIcon());
        Assert.assertNull(mTile.getIconTint());
    }

    @Test
    @SmallTest
    public void testBuildTestView_ModernSearch() {
        doReturn(true)
                .when(mMockTemplateUrlService)
                .isSearchResultsPageFromDefaultSearchProvider(any());

        buildTileView(TileStyle.MODERN);

        verify(mTileSetupCallback, times(0)).run();
        mPostTaskRunner.runAll();
        verify(mTileSetupCallback, times(1)).run();

        Assert.assertEquals(TileVisualType.ICON_DEFAULT, mTile.getType());
        Assert.assertNotEquals(mFakeColorStateList, mTile.getIconTint());

        ShadowDrawable shadowDrawable = shadowOf(mTile.getIcon());
        Assert.assertEquals(
                R.drawable.ic_suggestion_magnifier, shadowDrawable.getCreatedFromResId());
    }
}
