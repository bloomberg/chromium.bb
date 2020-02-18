// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;
import android.view.Menu;
import android.view.MenuItem;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests overview mode app menu popup.
 *
 * TODO(crbug.com/1031958): Add more required tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OverviewAppMenuTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().showOverview(true); });
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testAllMenuItemsWithoutStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureUtilities.setStartSurfaceEnabledForTesting(false);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_incognito_tabs_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(6));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testIncognitoAllMenuItemsWithoutStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            FeatureUtilities.setStartSurfaceEnabledForTesting(false);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_tabs_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(6));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testAllMenuItemsWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureUtilities.setStartSurfaceEnabledForTesting(true);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.all_bookmarks_menu_id
                        || itemId == R.id.recent_tabs_menu_id || itemId == R.id.open_history_menu_id
                        || itemId == R.id.downloads_menu_id || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_incognito_tabs_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(10));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testIncognitoAllMenuItemsWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
            FeatureUtilities.setStartSurfaceEnabledForTesting(true);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            int itemGroupId = item.getGroupId();
            if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                int itemId = item.getItemId();
                assertTrue(itemId == R.id.new_tab_menu_id
                        || itemId == R.id.new_incognito_tab_menu_id
                        || itemId == R.id.all_bookmarks_menu_id
                        || itemId == R.id.recent_tabs_menu_id || itemId == R.id.open_history_menu_id
                        || itemId == R.id.downloads_menu_id || itemId == R.id.close_all_tabs_menu_id
                        || itemId == R.id.close_all_incognito_tabs_menu_id
                        || itemId == R.id.menu_group_tabs || itemId == R.id.preferences_id);
                if (itemId == R.id.close_all_tabs_menu_id || itemId == R.id.recent_tabs_menu_id) {
                    assertFalse(item.isVisible());
                } else {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(10));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testGroupTabsIsDisabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureUtilities.setStartSurfaceEnabledForTesting(false);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testGroupTabsIsEnabled() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureUtilities.setStartSurfaceEnabledForTesting(false);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                int itemGroupId = item.getGroupId();
                if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                    assertTrue(item.isVisible());
                }
                if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                    assertFalse(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testGroupTabsIsDisabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureUtilities.setStartSurfaceEnabledForTesting(true);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                assertFalse(item.isVisible());
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }

    @Test
    @SmallTest
    @Feature({"Browser", "Main"})
    public void testGroupTabsIsEnabledWithStartSurface() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FeatureUtilities.setStartSurfaceEnabledForTesting(true);
            FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
            AppMenuTestSupport.showAppMenu(
                    mActivityTestRule.getAppMenuCoordinator(), null, false, false);
        });

        int checkedMenuItems = 0;
        Menu menu = mActivityTestRule.getMenu();
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.menu_group_tabs) {
                int itemGroupId = item.getGroupId();
                if (itemGroupId == R.id.OVERVIEW_MODE_MENU) {
                    assertFalse(item.isVisible());
                }
                if (itemGroupId == R.id.START_SURFACE_MODE_MENU) {
                    assertTrue(item.isVisible());
                }
                checkedMenuItems++;
            }
        }
        assertThat(checkedMenuItems, equalTo(2));
    }
}
