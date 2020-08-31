// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.util.Pair;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for reordering tabs in grid tab switcher in accessibility mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
public class TabGridAccessibilityHelperTest {
    // clang-format on
    @IntDef({TabMovementDirection.LEFT, TabMovementDirection.RIGHT, TabMovementDirection.UP,
            TabMovementDirection.DOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabMovementDirection {
        int LEFT = 0;
        int RIGHT = 1;
        int UP = 2;
        int DOWN = 3;
        int NUM_ENTRIES = 4;
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));
    }

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testGetPotentialActionsForView() {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final AccessibilityActionChecker checker = new AccessibilityActionChecker(cta);
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        assertTrue(cta.findViewById(R.id.tab_list_view)
                           instanceof TabListMediator.TabGridAccessibilityHelper);
        TabListMediator.TabGridAccessibilityHelper helper = cta.findViewById(R.id.tab_list_view);

        // Verify action list in portrait mode with span count = 2.
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check((v, noMatchingViewException) -> {
                    assertTrue(v instanceof RecyclerView);
                    RecyclerView recyclerView = (RecyclerView) v;

                    View item1 = getItemViewForPosition(recyclerView, 0);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item1),
                            new ArrayList<>(Arrays.asList(
                                    TabMovementDirection.RIGHT, TabMovementDirection.DOWN)));

                    View item2 = getItemViewForPosition(recyclerView, 1);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item2),
                            new ArrayList<>(Arrays.asList(
                                    TabMovementDirection.LEFT, TabMovementDirection.DOWN)));

                    View item3 = getItemViewForPosition(recyclerView, 2);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item3),
                            new ArrayList<>(Arrays.asList(TabMovementDirection.RIGHT,
                                    TabMovementDirection.UP, TabMovementDirection.DOWN)));

                    View item4 = getItemViewForPosition(recyclerView, 3);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item4),
                            new ArrayList<>(Arrays.asList(
                                    TabMovementDirection.LEFT, TabMovementDirection.UP)));

                    View item5 = getItemViewForPosition(recyclerView, 4);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item5),
                            new ArrayList<>(Arrays.asList(TabMovementDirection.UP)));
                });

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        // Verify action list in landscape mode with span count = 3.
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check((v, noMatchingViewException) -> {
                    assertTrue(v instanceof RecyclerView);
                    RecyclerView recyclerView = (RecyclerView) v;

                    View item1 = getItemViewForPosition(recyclerView, 0);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item1),
                            new ArrayList<>(Arrays.asList(
                                    TabMovementDirection.RIGHT, TabMovementDirection.DOWN)));

                    View item2 = getItemViewForPosition(recyclerView, 1);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item2),
                            new ArrayList<>(Arrays.asList(TabMovementDirection.LEFT,
                                    TabMovementDirection.RIGHT, TabMovementDirection.DOWN)));

                    View item3 = getItemViewForPosition(recyclerView, 2);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item3),
                            new ArrayList<>(Arrays.asList(TabMovementDirection.LEFT)));

                    View item4 = getItemViewForPosition(recyclerView, 3);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item4),
                            new ArrayList<>(Arrays.asList(
                                    TabMovementDirection.RIGHT, TabMovementDirection.UP)));

                    View item5 = getItemViewForPosition(recyclerView, 4);
                    checker.verifyListOfAccessibilityAction(
                            helper.getPotentialActionsForView(item5),
                            new ArrayList<>(Arrays.asList(
                                    TabMovementDirection.LEFT, TabMovementDirection.UP)));
                });
    }

    @Test
    @MediumTest
    public void testGetPositionsOfReorderAction() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        int leftActionId = R.id.move_tab_left;
        int rightActionId = R.id.move_tab_right;
        int upActionId = R.id.move_tab_up;
        int downActionId = R.id.move_tab_down;
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        assertTrue(cta.findViewById(R.id.tab_list_view)
                           instanceof TabListMediator.TabGridAccessibilityHelper);
        TabListMediator.TabGridAccessibilityHelper helper = cta.findViewById(R.id.tab_list_view);

        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check((v, noMatchingViewException) -> {
                    assertTrue(v instanceof RecyclerView);
                    RecyclerView recyclerView = (RecyclerView) v;
                    Pair<Integer, Integer> positions;

                    View item1 = getItemViewForPosition(recyclerView, 0);
                    positions = helper.getPositionsOfReorderAction(item1, rightActionId);
                    assertEquals(0, (int) positions.first);
                    assertEquals(1, (int) positions.second);

                    positions = helper.getPositionsOfReorderAction(item1, downActionId);
                    assertEquals(0, (int) positions.first);
                    assertEquals(2, (int) positions.second);

                    View item4 = getItemViewForPosition(recyclerView, 3);
                    positions = helper.getPositionsOfReorderAction(item4, leftActionId);
                    assertEquals(3, (int) positions.first);
                    assertEquals(2, (int) positions.second);

                    positions = helper.getPositionsOfReorderAction(item4, upActionId);
                    assertEquals(3, (int) positions.first);
                    assertEquals(1, (int) positions.second);
                });

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check((v, noMatchingViewException) -> {
                    assertTrue(v instanceof RecyclerView);
                    RecyclerView recyclerView = (RecyclerView) v;
                    Pair<Integer, Integer> positions;

                    View item2 = getItemViewForPosition(recyclerView, 1);
                    positions = helper.getPositionsOfReorderAction(item2, leftActionId);
                    assertEquals(1, (int) positions.first);
                    assertEquals(0, (int) positions.second);

                    positions = helper.getPositionsOfReorderAction(item2, rightActionId);
                    assertEquals(1, (int) positions.first);
                    assertEquals(2, (int) positions.second);

                    positions = helper.getPositionsOfReorderAction(item2, downActionId);
                    assertEquals(1, (int) positions.first);
                    assertEquals(4, (int) positions.second);

                    View item5 = getItemViewForPosition(recyclerView, 4);
                    positions = helper.getPositionsOfReorderAction(item5, leftActionId);
                    assertEquals(4, (int) positions.first);
                    assertEquals(3, (int) positions.second);

                    positions = helper.getPositionsOfReorderAction(item5, upActionId);
                    assertEquals(4, (int) positions.first);
                    assertEquals(1, (int) positions.second);
                });
    }

    private View getItemViewForPosition(RecyclerView recyclerView, int position) {
        RecyclerView.ViewHolder viewHolder =
                recyclerView.findViewHolderForAdapterPosition(position);
        assertNotNull(viewHolder);
        return viewHolder.itemView;
    }

    private static class AccessibilityActionChecker {
        private final Context mContext;

        AccessibilityActionChecker(ChromeTabbedActivity cta) {
            mContext = cta;
        }

        void verifyListOfAccessibilityAction(
                List<AccessibilityAction> actions, List<Integer> directions) {
            assertEquals(directions.size(), actions.size());
            for (int i = 0; i < actions.size(); i++) {
                verifyAccessibilityAction(actions.get(i), directions.get(i));
            }
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        void verifyAccessibilityAction(
                AccessibilityAction action, @TabMovementDirection int direction) {
            switch (direction) {
                case TabMovementDirection.LEFT:
                    assertEquals(R.id.move_tab_left, action.getId());
                    assertEquals(mContext.getString(R.string.accessibility_tab_movement_left),
                            action.getLabel());
                    break;
                case TabMovementDirection.RIGHT:
                    assertEquals(R.id.move_tab_right, action.getId());
                    assertEquals(mContext.getString(R.string.accessibility_tab_movement_right),
                            action.getLabel());
                    break;
                case TabMovementDirection.UP:
                    assertEquals(R.id.move_tab_up, action.getId());
                    assertEquals(mContext.getString(R.string.accessibility_tab_movement_up),
                            action.getLabel());
                    break;
                case TabMovementDirection.DOWN:
                    assertEquals(R.id.move_tab_down, action.getId());
                    assertEquals(mContext.getString(R.string.accessibility_tab_movement_down),
                            action.getLabel());
                    break;
                default:
                    assert false;
            }
        }
    }
}
