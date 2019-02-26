// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetProperties.VISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Tab;
import org.chromium.chrome.browser.modelutil.LazyConstructionPropertyMcp;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.ui.DeferredViewStubInflationProvider;
import org.chromium.ui.ViewProvider;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutionException;

/**
 * View tests for the keyboard accessory sheet component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetViewTest {
    private PropertyModel mModel;
    private BlockingQueue<AccessorySheetView> mViewPager;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ViewStub viewStub = mActivityTestRule.getActivity().findViewById(
                    R.id.keyboard_accessory_sheet_stub);
            int height = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_sheet_height);
            mModel = new PropertyModel
                             .Builder(TABS, ACTIVE_TAB_INDEX, VISIBLE, HEIGHT, TOP_SHADOW_VISIBLE)
                             .with(HEIGHT, height)
                             .with(TABS, new ListModel<>())
                             .with(ACTIVE_TAB_INDEX, NO_ACTIVE_TAB)
                             .with(VISIBLE, false)
                             .with(TOP_SHADOW_VISIBLE, false)
                             .build();
            ViewProvider<AccessorySheetView> provider =
                    new DeferredViewStubInflationProvider<>(viewStub);
            mViewPager = new ArrayBlockingQueue<>(1);
            LazyConstructionPropertyMcp.create(
                    mModel, VISIBLE, provider, AccessorySheetViewBinder::bind);
            provider.whenLoaded(mViewPager::add);
        });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel()
            throws ExecutionException, InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mViewPager.poll());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, true); });
        AccessorySheetView viewPager = mViewPager.take();
        assertEquals(viewPager.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, false); });
        assertNotEquals(viewPager.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testAddingTabToModelRendersTabsView() throws InterruptedException {
        final String kSampleAction = "Some Action";
        mModel.get(TABS).add(new Tab(null, null, R.layout.empty_accessory_sheet,
                AccessoryTabType.PASSWORDS, new Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        assertNotNull("The tab must have been created!", view);
                        assertTrue("Empty tab is a layout.", view instanceof LinearLayout);
                        LinearLayout baseLayout = (LinearLayout) view;
                        TextView sampleTextView = new TextView(mActivityTestRule.getActivity());
                        sampleTextView.setText(kSampleAction);
                        baseLayout.addView(sampleTextView);
                    }

                    @Override
                    public void onTabShown() {}
                }));
        mModel.set(ACTIVE_TAB_INDEX, 0);
        // Shouldn't cause the view to be inflated.
        assertNull(mViewPager.poll());

        // Setting visibility should cause the Tab to be rendered.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        assertNotNull(mViewPager.take());

        onView(withText(kSampleAction)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSettingActiveTabIndexChangesTab() {
        final String kFirstTab = "First Tab";
        final String kSecondTab = "Second Tab";
        mModel.get(TABS).add(createTestTabWithTextView(kFirstTab));
        mModel.get(TABS).add(createTestTabWithTextView(kSecondTab));
        mModel.set(ACTIVE_TAB_INDEX, 0);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true)); // Render view.

        onView(withText(kFirstTab)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(ACTIVE_TAB_INDEX, 1));

        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, withText(kSecondTab)));
    }

    @Test
    @MediumTest
    public void testRemovingTabDeletesItsView() {
        final String kFirstTab = "First Tab";
        final String kSecondTab = "Second Tab";
        mModel.get(TABS).add(createTestTabWithTextView(kFirstTab));
        mModel.get(TABS).add(createTestTabWithTextView(kSecondTab));
        mModel.set(ACTIVE_TAB_INDEX, 0);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true)); // Render view.

        onView(withText(kFirstTab)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.get(TABS).remove(mModel.get(TABS).get(0)));

        onView(withText(kFirstTab)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testReplaceLastTab() {
        final String kFirstTab = "First Tab";
        mModel.get(TABS).add(createTestTabWithTextView(kFirstTab));
        mModel.set(ACTIVE_TAB_INDEX, 0);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true)); // Render view.

        // Remove the last tab.
        onView(withText(kFirstTab)).check(matches(isDisplayed()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.get(TABS).remove(mModel.get(TABS).get(0)); });
        onView(withText(kFirstTab)).check(doesNotExist());

        // Add a new first tab.
        final String kSecondTab = "Second Tab";
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView(kSecondTab));
            mModel.set(ACTIVE_TAB_INDEX, 0);
        });
        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, withText(kSecondTab)));
    }

    @Test
    @MediumTest
    public void testTopShadowVisiblitySetByModel() {
        mModel.get(TABS).add(createTestTabWithTextView("SomeTab"));
        mModel.set(TOP_SHADOW_VISIBLE, false);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true)); // Render view.
        onView(isRoot()).check((r, e) -> {
            waitForView(
                    (ViewGroup) r, withId(R.id.accessory_sheet_shadow), ViewUtils.VIEW_INVISIBLE);
        });

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(TOP_SHADOW_VISIBLE, true));
        onView(withId(R.id.accessory_sheet_shadow)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(TOP_SHADOW_VISIBLE, false));
        onView(isRoot()).check((r, e) -> {
            waitForView(
                    (ViewGroup) r, withId(R.id.accessory_sheet_shadow), ViewUtils.VIEW_INVISIBLE);
        });
    }

    private Tab createTestTabWithTextView(String textViewCaption) {
        return new Tab(null, null, R.layout.empty_accessory_sheet, AccessoryTabType.PASSWORDS,
                new Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        TextView sampleTextView = new TextView(mActivityTestRule.getActivity());
                        sampleTextView.setText(textViewCaption);
                        view.addView(sampleTextView);
                    }

                    @Override
                    public void onTabShown() {}
                });
    }
}