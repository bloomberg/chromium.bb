// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.view.ContextThemeWrapper;

import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.device.mojom.ScreenOrientationLockType;

import java.util.ArrayList;
import java.util.Collections;

/** Tests for {@link CustomTabIntentDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabIntentDataProviderTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String BUTTON_DESCRIPTION = "buttonDescription";

    private Context mContext;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.ColorOverlay);
    }

    @Test
    public void colorSchemeParametersAreRetrieved() {
        CustomTabColorSchemeParams lightParams = new CustomTabColorSchemeParams.Builder()
                .setToolbarColor(0xff0000ff)
                .setSecondaryToolbarColor(0xff00aaff)
                .setNavigationBarColor(0xff112233)
                .build();
        CustomTabColorSchemeParams darkParams = new CustomTabColorSchemeParams.Builder()
                .setToolbarColor(0xffff0000)
                .setSecondaryToolbarColor(0xffff8800)
                .setNavigationBarColor(0xff332211)
                .build();
        Intent intent = new CustomTabsIntent.Builder()
                .setColorSchemeParams(COLOR_SCHEME_LIGHT, lightParams)
                .setColorSchemeParams(COLOR_SCHEME_DARK, darkParams)
                .build()
                .intent;

        ColorProvider lightProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT)
                        .getColorProvider();
        ColorProvider darkProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_DARK)
                        .getColorProvider();

        assertEquals((int) lightParams.toolbarColor, lightProvider.getToolbarColor());
        assertEquals((int) darkParams.toolbarColor, darkProvider.getToolbarColor());

        assertEquals((int) lightParams.secondaryToolbarColor, lightProvider.getBottomBarColor());
        assertEquals((int) darkParams.secondaryToolbarColor, darkProvider.getBottomBarColor());

        assertEquals(lightParams.navigationBarColor, lightProvider.getNavigationBarColor());
        assertEquals(darkParams.navigationBarColor, darkProvider.getNavigationBarColor());
    }

    /* Test the setting the default orientation for Trusted Web Activity and getting the default
     * orientation.
     */
    @Test
    public void defaultOrientationIsSet() {
        CustomTabsSession mSession = CustomTabsSession.createMockSessionForTesting(
                new ComponentName(mContext, ChromeLauncherActivity.class));

        TrustedWebActivityIntentBuilder twaBuilder =
                new TrustedWebActivityIntentBuilder(getLaunchingUrl())
                        .setScreenOrientation(ScreenOrientation.LANDSCAPE);
        Intent intent = twaBuilder.build(mSession).getIntent();
        CustomTabIntentDataProvider customTabIntentDataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(ScreenOrientationLockType.LANDSCAPE,
                customTabIntentDataProvider.getDefaultOrientation());

        twaBuilder = new TrustedWebActivityIntentBuilder(getLaunchingUrl())
                             .setScreenOrientation(ScreenOrientation.PORTRAIT);
        intent = twaBuilder.build(mSession).getIntent();
        customTabIntentDataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
        assertEquals(ScreenOrientationLockType.PORTRAIT,
                customTabIntentDataProvider.getDefaultOrientation());
    }

    @Test
    public void shareStateDefault_noButtonInToolbar_hasShareInToolbar() {
        Intent intent = new Intent().putExtra(
                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_DEFAULT);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(mContext.getResources().getString(R.string.share),
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
    }

    @Test
    public void shareStateDefault_buttonInToolbar_hasShareItemInMenu() {
        Intent intent = new Intent()
                                .putExtra(CustomTabsIntent.EXTRA_SHARE_STATE,
                                        CustomTabsIntent.SHARE_STATE_DEFAULT)
                                .putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                                        createActionButtonInToolbarBundle());

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateDefault_buttonInToolbarAndCustomMenuItems_hasNoShare() {
        Intent intent =
                new Intent()
                        .putExtra(CustomTabsIntent.EXTRA_SHARE_STATE,
                                CustomTabsIntent.SHARE_STATE_DEFAULT)
                        .putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE,
                                createActionButtonInToolbarBundle())
                        .putExtra(CustomTabsIntent.EXTRA_MENU_ITEMS,
                                new ArrayList<>(Collections.singletonList(createMenuItemBundle())));

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOn_buttonInToolbar_hasShareItemInMenu() {
        ArrayList<Bundle> buttons =
                new ArrayList<>(Collections.singleton(createActionButtonInToolbarBundle()));
        Intent intent = new Intent()
                                .putExtra(CustomTabsIntent.EXTRA_SHARE_STATE,
                                        CustomTabsIntent.SHARE_STATE_ON)
                                .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertTrue(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOn_buttonInToolbarAndCustomMenuItems_hasNoShare() {
        ArrayList<Bundle> buttons =
                new ArrayList<>(Collections.singleton(createActionButtonInToolbarBundle()));
        Intent intent =
                new Intent()
                        .putExtra(
                                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_ON)
                        .putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, buttons)
                        .putExtra(CustomTabsIntent.EXTRA_MENU_ITEMS,
                                new ArrayList<>(Collections.singletonList(createMenuItemBundle())));

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertEquals(BUTTON_DESCRIPTION,
                dataProvider.getCustomButtonsOnToolbar().get(0).getDescription());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    @Test
    public void shareStateOff_noShareItems() {
        Intent intent = new Intent().putExtra(
                CustomTabsIntent.EXTRA_SHARE_STATE, CustomTabsIntent.SHARE_STATE_OFF);

        CustomTabIntentDataProvider dataProvider =
                new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);

        assertTrue(dataProvider.getCustomButtonsOnToolbar().isEmpty());
        assertFalse(dataProvider.shouldShowShareMenuItem());
    }

    private Bundle createActionButtonInToolbarBundle() {
        Bundle bundle = new Bundle();
        bundle.putInt(CustomTabsIntent.KEY_ID, CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID);
        int iconHeight = mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        bundle.putParcelable(CustomTabsIntent.KEY_ICON,
                Bitmap.createBitmap(iconHeight, iconHeight, Bitmap.Config.ALPHA_8));
        bundle.putString(CustomTabsIntent.KEY_DESCRIPTION, BUTTON_DESCRIPTION);
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(mContext, 0, new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        bundle.putBoolean(CustomButtonParamsImpl.SHOW_ON_TOOLBAR, true);
        return bundle;
    }

    private Bundle createMenuItemBundle() {
        Bundle bundle = new Bundle();
        bundle.putString(CustomTabsIntent.KEY_MENU_ITEM_TITLE, "title");
        bundle.putParcelable(CustomTabsIntent.KEY_PENDING_INTENT,
                PendingIntent.getBroadcast(mContext, 0, new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(true)));
        return bundle;
    }

    protected Uri getLaunchingUrl() {
        return Uri.parse("https://www.example.com/");
    }
}
