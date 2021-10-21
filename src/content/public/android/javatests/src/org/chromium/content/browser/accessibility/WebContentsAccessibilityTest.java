// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static android.content.Context.CLIPBOARD_SERVICE;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_ARGUMENT_HTML_ELEMENT_STRING;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_CLICK;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_COPY;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_CUT;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_PASTE;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_PREVIOUS_HTML_ELEMENT;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_ACCESSIBILITY_FOCUS;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_CLEAR_ACCESSIBILITY_FOCUS;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_LONG_CLICK;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_DOWN;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_LEFT;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_RIGHT;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_UP;
import static android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_TEXT;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX;
import static android.view.accessibility.AccessibilityNodeInfo.EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY;

import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.NODE_TIMEOUT_ERROR;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sClassNameMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sInputTypeMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sRangeInfoMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sTextMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sTextOrContentDescriptionMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sViewIdResourceNameMatcher;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.ACTION_ARGUMENT_PROGRESS_VALUE;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.ACTION_SET_PROGRESS;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EVENTS_DROPPED_HISTOGRAM;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_CHROME_ROLE;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_OFFSCREEN;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_UNCLIPPED_BOTTOM;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.EXTRAS_KEY_UNCLIPPED_TOP;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.ONE_HUNDRED_PERCENT_HISTOGRAM;
import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.PERCENTAGE_DROPPED_HISTOGRAM;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.text.Spannable;
import android.text.style.SuggestionSpan;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for WebContentsAccessibility. Actually tests WebContentsAccessibilityImpl that
 * implements the interface.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
@SuppressLint("VisibleForTests")
public class WebContentsAccessibilityTest {
    // Test output error messages
    private static final String COMBOBOX_ERROR = "expanded combobox announcement was incorrect.";
    private static final String DISABLED_COMBOBOX_ERROR =
            "disabled combobox child elements should not be clickable";
    private static final String LONG_CLICK_ERROR =
            "node should not have the ACTION_LONG_CLICK action as an available action";
    private static final String ACTION_SET_ERROR =
            "node should have the ACTION_SET_TEXT action as an available action";
    private static final String THRESHOLD_ERROR =
            "Too many TYPE_WINDOW_CONTENT_CHANGED events received in an atomic update.";
    private static final String THRESHOLD_LOW_EVENT_COUNT_ERROR =
            "Expected more TYPE_WINDOW_CONTENT_CHANGED events"
            + "in an atomic update, is throttling still necessary?";
    private static final String ARIA_INVALID_ERROR =
            "Error message for aria-invalid node has not been set correctly.";
    private static final String CONTENTEDITABLE_ERROR =
            "contenteditable node is not being identified and/or received incorrect class name";
    private static final String SPELLING_ERROR =
            "node should have a Spannable with spelling correction for given text.";
    private static final String INPUT_RANGE_VALUE_MISMATCH =
            "Value for <input type='range'> is incorrect, did you honor 'step' value?";
    private static final String INPUT_RANGE_VALUETEXT_MISMATCH =
            "Value for <input type='range'> text is incorrect, did you honor aria-valuetext?";
    private static final String INPUT_RANGE_EVENT_ERROR =
            "TYPE_VIEW_SCROLLED event not received before timeout.";
    private static final String CACHING_ERROR = "AccessibilityNodeInfo cache has stale data";
    private static final String NODE_EXTRAS_UNCLIPPED_ERROR =
            "AccessibilityNodeInfo object should have unclipped bounds in extras bundle";
    private static final String EVENT_TYPE_MASK_ERROR =
            "Conversion of event masks to event types not correct.";
    private static final String TEXT_SELECTION_AND_TRAVERSAL_ERROR =
            "Expected to receive both a traversal and selection text event";
    private static final String BOUNDING_BOX_ERROR =
            "Expected bounding box to change after web contents was resized.";
    private static final String ONDEMAND_HISTOGRAM_ERROR =
            "Expected histogram for OnDemand AT feature to be recorded.";
    private static final String VISIBLE_TO_USER_ERROR =
            "AccessibilityNodeInfo object has incorrect visibleToUser value";
    private static final String OFFSCREEN_BUNDLE_EXTRA_ERROR =
            "AccessibilityNodeInfo object has incorrect Bundle extras for offscreen boolean.";
    private static final String PERFORM_ACTION_ERROR =
            "performAction did not update node as expected.";

    // Constant values for unit tests
    private static final int UNSUPPRESSED_EXPECTED_COUNT = 15;

    private AccessibilityNodeInfo mNodeInfo;
    private AccessibilityContentShellTestData mTestData;

    @Rule
    public AccessibilityContentShellActivityTestRule mActivityTestRule =
            new AccessibilityContentShellActivityTestRule();

    /**
     * Helper methods for setup of a basic web contents accessibility unit test.
     *
     * These methods replace the usual setUp() method annotated with @Before because we wish to
     * load different data with each test, but the process is the same for all tests.
     *
     * Leaving a commented @Before annotation on each method as a reminder/context clue.
     */
    /* @Before */
    protected void setupTestWithHTML(String html) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFramework();
        mActivityTestRule.setAccessibilityDelegate();

        mTestData = AccessibilityContentShellTestData.getInstance();
        mActivityTestRule.sendReadyForTestSignal();
    }

    /* @Before */
    protected void setupTestFromFile(String filepath) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.getIsolatedTestFileUrl(filepath));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFramework();
        mActivityTestRule.setAccessibilityDelegate();

        mTestData = AccessibilityContentShellTestData.getInstance();
        mActivityTestRule.sendReadyForTestSignal();
    }

    /**
     * Helper method to tear down our tests so we can start the next test clean.
     */
    @After
    public void tearDown() {
        mTestData = null;
        mNodeInfo = null;
    }

    // Helper pass-through methods to make tests easier to read.
    private <T> int waitForNodeMatching(
            AccessibilityContentShellTestUtils.AccessibilityNodeInfoMatcher<T> matcher, T element) {
        return mActivityTestRule.waitForNodeMatching(matcher, element);
    }

    private boolean performActionOnUiThread(int viewId, int action, Bundle args)
            throws ExecutionException {
        return mActivityTestRule.performActionOnUiThread(viewId, action, args);
    }

    private boolean performActionOnUiThread(int viewId, int action, Bundle args,
            Callable<Boolean> criteria) throws ExecutionException, Throwable {
        return mActivityTestRule.performActionOnUiThread(viewId, action, args, criteria);
    }

    private void executeJS(String method) {
        mActivityTestRule.executeJS(method);
    }

    private void focusNode(int virtualViewId) throws Throwable {
        mActivityTestRule.focusNode(virtualViewId);
    }

    public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualViewId) {
        return mActivityTestRule.mNodeProvider.createAccessibilityNodeInfo(virtualViewId);
    }

    /**
     * Helper method for sending text related events and confirming that the associated text
     * selection and traversal events have been dispatched before continuing with test.
     *
     * @param viewId            int virtualViewId of the text field
     * @param action            int action to perform
     * @param args              Bundle optional arguments
     * @throws ExecutionException   Error
     */
    private void performTextActionOnUiThread(int viewId, int action, Bundle args)
            throws ExecutionException {
        // Reset values for traversal and selection events.
        mTestData.setReceivedTraversalEvent(false);
        mTestData.setReceivedSelectionEvent(false);

        // Perform our text selection/traversal action.
        mActivityTestRule.performActionOnUiThread(viewId, action, args);

        // Poll until both events have been confirmed as received
        CriteriaHelper.pollUiThread(() -> {
            return mTestData.hasReceivedTraversalEvent() && mTestData.hasReceivedSelectionEvent();
        }, TEXT_SELECTION_AND_TRAVERSAL_ERROR);
    }

    /**
     * Test <input type="range"> nodes and events for incrementing/decrementing value with actions.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testAccessibilityNodeInfo_inputTypeRange() throws Throwable {
        // Create a basic input range, and find the associated |AccessibilityNodeInfo| object.
        setupTestWithHTML("<input type='range' min='0' max='40'>");

        // Find the input range and assert we have the correct node.
        int inputNodeVirtualViewId = waitForNodeMatching(sRangeInfoMatcher, "");
        mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.001);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 40, mNodeInfo.getRangeInfo().getMax(), 0.001);

        // Perform a series of slider increments and check results.
        for (int i = 1; i <= 10; i++) {
            // Increment our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId,
                    AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(INPUT_RANGE_VALUE_MISMATCH, 20 + (2 * i),
                    mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }

        // Perform a series of slider decrements and check results.
        for (int i = 1; i <= 20; i++) {
            // Decrement our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId,
                    AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(INPUT_RANGE_VALUE_MISMATCH, 40 - (2 * i),
                    mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }
    }

    /**
     * Ensure we are honoring min/max/step values for <input type="range"> nodes.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testAccessibilityNodeInfo_inputTypeRange_withStepValue() throws Throwable {
        // Create a basic input range, and find the associated |AccessibilityNodeInfo| object.
        setupTestWithHTML("<input type='range' min='0' max='144' step='12'>");

        // Find the input range and assert we have the correct node.
        int inputNodeVirtualViewId = waitForNodeMatching(sRangeInfoMatcher, "");
        mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.001);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 144, mNodeInfo.getRangeInfo().getMax(), 0.001);

        // Perform a series of slider increments and check results.
        int[] expectedVals = new int[] {84, 96, 108, 120, 132, 144};
        for (int expectedVal : expectedVals) {
            // Increment our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId,
                    AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(INPUT_RANGE_VALUE_MISMATCH, expectedVal,
                    mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }

        // Perform a series of slider decrements and check results.
        expectedVals = new int[] {132, 120, 108, 96, 84, 72, 60, 48, 36, 24, 12, 0};
        for (int expectedVal : expectedVals) {
            // Decrement our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId,
                    AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(INPUT_RANGE_VALUE_MISMATCH, expectedVal,
                    mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }
    }

    /**
     * Test <input type="range"> nodes move by a minimum value with increment/decrement actions.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testAccessibilityNodeInfo_inputTypeRange_withRequiredMin() throws Throwable {
        // Create a basic input range, and find the associated |AccessibilityNodeInfo| object.
        setupTestWithHTML("<input type='range' min='0' max='1000' step='1'>");

        // Find the input range and assert we have the correct node.
        int inputNodeVirtualViewId = waitForNodeMatching(sRangeInfoMatcher, "");
        mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.001);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, 1000, mNodeInfo.getRangeInfo().getMax(), 0.001);

        // Perform a series of slider increments and check results.
        for (int i = 1; i <= 10; i++) {
            // Increment our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId,
                    AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(INPUT_RANGE_VALUE_MISMATCH, 500 + (10 * i),
                    mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }

        // Perform a series of slider decrements and check results.
        for (int i = 1; i <= 20; i++) {
            // Decrement our slider using action, and poll until we receive the scroll event.
            performActionOnUiThread(inputNodeVirtualViewId,
                    AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD, new Bundle());
            CriteriaHelper.pollUiThread(
                    () -> mTestData.hasReceivedEvent(), INPUT_RANGE_EVENT_ERROR);

            // Refresh our node info to get the latest RangeInfo child object.
            mNodeInfo = createAccessibilityNodeInfo(inputNodeVirtualViewId);

            // Confirm slider values.
            Assert.assertEquals(INPUT_RANGE_VALUE_MISMATCH, 600 - (10 * i),
                    mNodeInfo.getRangeInfo().getCurrent(), 0.001);

            // Reset polling value for next test
            mTestData.setReceivedEvent(false);
        }
    }

    /**
     * Test <input type="range"> nodes are properly populated when aria-valuetext is set.
     */
    @Test
    @SmallTest
    public void testAccessibilityNodeInfo_inputTypeRange_withAriaValueText() {
        // Build a simple web page with input nodes that have aria-valuetext.
        setupTestWithHTML(
                "<input id='in1' type='range' value='1' min='0' max='2' aria-valuetext='medium'>"
                + "<label for='in2'>This is a test label"
                + "  <input id='in2' type='range' value='0' min='0' max='2' aria-valuetext='small'>"
                + "</label>");

        int vvIdInput1 = waitForNodeMatching(sViewIdResourceNameMatcher, "in1");
        int vvIdInput2 = waitForNodeMatching(sViewIdResourceNameMatcher, "in2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvIdInput1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvIdInput2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        mActivityTestRule.sendEndOfTestSignal();

        // Check the text of each element, and that RangeInfo has not been set.
        mNodeInfo1 = createAccessibilityNodeInfo(vvIdInput1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvIdInput2);
        Assert.assertEquals(
                INPUT_RANGE_VALUETEXT_MISMATCH, "medium", mNodeInfo1.getText().toString());
        Assert.assertEquals(INPUT_RANGE_VALUETEXT_MISMATCH, "small, This is a test label",
                mNodeInfo2.getText().toString());
        Assert.assertNull(INPUT_RANGE_VALUETEXT_MISMATCH, mNodeInfo1.getRangeInfo());
        Assert.assertNull(INPUT_RANGE_VALUETEXT_MISMATCH, mNodeInfo2.getRangeInfo());
    }

    /**
     * Ensure we throttle TYPE_WINDOW_CONTENT_CHANGED events for large tree updates.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testMaxContentChangedEventsFired_default() throws Throwable {
        // Build a simple web page with complex visibility change.
        setupTestFromFile("content/test/data/android/type_window_content_changed_events.html");

        // Determine the current max events to fire
        int maxEvents = mActivityTestRule.mWcax.getMaxContentChangedEventsToFireForTesting();

        // Find the button node.
        int vvid = waitForNodeMatching(sClassNameMatcher, "android.widget.Button");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Expand All", mNodeInfo.getText());

        // Run JS code to expand comboboxes.
        executeJS("expandComboboxes()");

        // Poll until the JS method is confirmed to have finished.
        CriteriaHelper.pollUiThread(() -> {
            return createAccessibilityNodeInfo(vvid).getText().toString().equals("Done");
        }, NODE_TIMEOUT_ERROR);

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // Verify number of events processed, allow for multiple atomic updates.
        int eventCount = mTestData.getTypeWindowContentChangedCount();
        Assert.assertTrue(thresholdError(eventCount, maxEvents), eventCount <= (maxEvents * 3));
    }

    /**
     * Ensure we need to throttle TYPE_WINDOW_CONTENT_CHANGED events for some large tree updates.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testMaxContentChangedEventsFired_largeLimit() throws Throwable {
        // Build a simple web page with complex visibility change.
        setupTestFromFile("content/test/data/android/type_window_content_changed_events.html");

        // "Disable" event suppression by setting an arbitrarily high max events value.
        mActivityTestRule.mWcax.setMaxContentChangedEventsToFireForTesting(Integer.MAX_VALUE);

        // Find the button node.
        int vvid = waitForNodeMatching(sClassNameMatcher, "android.widget.Button");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Expand All", mNodeInfo.getText());

        // Run JS code to expand comboboxes.
        executeJS("expandComboboxes()");

        // Poll until the JS method is confirmed to have finished.
        CriteriaHelper.pollUiThread(() -> {
            return createAccessibilityNodeInfo(vvid).getText().toString().equals("Done");
        }, NODE_TIMEOUT_ERROR);

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // Verify number of events processed
        int eventCount = mTestData.getTypeWindowContentChangedCount();
        Assert.assertTrue(lowThresholdError(eventCount), eventCount > UNSUPPRESSED_EXPECTED_COUNT);
    }

    /**
     * Ensure we send an announcement on combobox expansion.
     */
    @Test
    @SmallTest
    public void testEventText_Combobox() throws Throwable {
        // Build a simple web page with a combobox, and focus the input field.
        setupTestFromFile("content/test/data/android/input/input_combobox.html");

        // Find a node in the accessibility tree of the correct class.
        int comboBoxVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(comboBoxVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(comboBoxVirtualViewId);

        // Run JS code to expand the combobox
        executeJS("expandCombobox()");

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // We should have received a TYPE_ANNOUNCEMENT event, check announcement text.
        Assert.assertEquals(COMBOBOX_ERROR, "expanded, 3 autocomplete options available.",
                mTestData.getAnnouncementText());
    }

    /**
     * Ensure we send an announcement on combobox expansion that opens a dialog.
     */
    @Test
    @SmallTest
    public void testEventText_Combobox_dialog() throws Throwable {
        // Build a simple web page with a combobox, and focus the input field.
        setupTestFromFile("content/test/data/android/input/input_combobox_dialog.html");

        // Find a node in the accessibility tree of the correct class.
        int comboBoxVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(comboBoxVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(comboBoxVirtualViewId);

        // Run JS code to expand the combobox
        executeJS("expandCombobox()");

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // We should have received a TYPE_ANNOUNCEMENT event, check announcement text.
        Assert.assertEquals(
                COMBOBOX_ERROR, "expanded, dialog opened.", mTestData.getAnnouncementText());
    }

    /**
     * Ensure we send an announcement on combobox expansion with aria-1.0 spec.
     */
    @Test
    @SmallTest
    public void testEventText_Combobox_ariaOne() throws Throwable {
        // Build a simple web page with a combobox, and focus the input field.
        setupTestFromFile("content/test/data/android/input/input_combobox_aria1.0.html");

        // Find a node in the accessibility tree of the correct class.
        int comboBoxVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(comboBoxVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(comboBoxVirtualViewId);

        // Run JS code to expand the combobox
        executeJS("expandCombobox()");

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // We should have received a TYPE_ANNOUNCEMENT event, check announcement text.
        Assert.assertEquals(COMBOBOX_ERROR, "expanded, 3 autocomplete options available.",
                mTestData.getAnnouncementText());
    }

    /**
     * Ensure that disabled comboboxes and children are not shadow clickable.
     */
    @Test
    @SmallTest
    public void testEvent_Combobox_disabled() throws Throwable {
        // Build a simple web page with a disabled combobox.
        setupTestWithHTML("<select disabled>\n"
                + "  <option>Volvo</option>\n"
                + "  <option>Saab</option>\n"
                + "  <option>Mercedes</option>\n"
                + "</select>");

        // Find the disabled option node and set a delegate to track focus.
        int disabledNodeId = waitForNodeMatching(sTextMatcher, "Volvo");
        mNodeInfo = createAccessibilityNodeInfo(disabledNodeId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(disabledNodeId);
        mTestData.setReceivedAccessibilityFocusEvent(false);

        // Perform a click on the node.
        performActionOnUiThread(disabledNodeId, ACTION_CLICK, null);

        // Signal end of test
        mActivityTestRule.sendEndOfTestSignal();

        // Check we did not receive any events.
        Assert.assertFalse(DISABLED_COMBOBOX_ERROR, mTestData.hasReceivedAccessibilityFocusEvent());
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by character with selection mode off
     */
    @Test
    @SmallTest
    public void testEventIndices_SelectionOFF_CharacterGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing"
        setupTestWithHTML("<input id=\"fn\" type=\"text\" value=\"Testing\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to CHARACTER, with selection FALSE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        // Simulate swiping left (backward)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by character with selection mode on
     */
    @Test
    @LargeTest
    public void testEventIndices_SelectionON_CharacterGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing"
        setupTestWithHTML("<input id=\"fn\" type=\"text\" value=\"Testing\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to CHARACTER, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping left (backward) (adds to selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward) (removes from selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by word with selection mode off
     */
    @Test
    @SmallTest
    public void testEventIndices_SelectionOFF_WordGranularity() throws Throwable {
        // Build a simple web page with an input and the text "Testing this output is correct"
        setupTestWithHTML(
                "<input id=\"fn\" type=\"text\" value=\"Testing this output is correct\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to WORD, with selection FALSE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);

        int[] wordStarts = new int[] {0, 8, 13, 20, 23};
        int[] wordEnds = new int[] {7, 12, 19, 22, 30};

        // Simulate swiping left (backward) through all 5 words, check indices along the way
        for (int i = 4; i >= 0; --i) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward) through all 5 words, check indices along the way
        for (int i = 0; i < 5; ++i) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating an edit
     * field by word with selection mode on
     */
    @Test
    @LargeTest
    public void testEventIndices_SelectionON_WordGranularity() throws Throwable {
        setupTestWithHTML(
                "<input id=\"fn\" type=\"text\" value=\"Testing this output is correct\">");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int editTextVirtualViewId =
                waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(editTextVirtualViewId);
        Assert.assertNotEquals(mNodeInfo, null);

        focusNode(editTextVirtualViewId);

        // Set granularity to WORD, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        int[] wordStarts = new int[] {0, 8, 13, 20, 23};
        int[] wordEnds = new int[] {7, 12, 19, 22, 30};

        // Simulate swiping left (backward, adds to selection) through all 5 words, check indices
        for (int i = 4; i >= 0; --i) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(30, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward, removes selection) through all 5 words, check indices
        for (int i = 0; i < 5; ++i) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(30, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionToIndex());
        }

        // Turn selection mode off and traverse to beginning so we can select forwards
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 4; i >= 0; i--) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 5; ++i) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getSelectionToIndex());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 4; i >= 0; --i) {
            performTextActionOnUiThread(editTextVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(wordStarts[i], mTestData.getTraverseFromIndex());
            Assert.assertEquals(wordEnds[i], mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(wordStarts[i], mTestData.getSelectionToIndex());
        }
    }

    /**
     * Ensure traverse events and selection events are properly indexed when navigating a
     * contenteditable by character with selection mode on.
     */
    @Test
    @LargeTest
    public void testEventIndices_contenteditable_SelectionON_CharacterGranularity()
            throws Throwable {
        setupTestWithHTML("<div contenteditable>Testing</div>");

        // Find a node in the accessibility tree with input type TYPE_CLASS_TEXT.
        int contentEditableVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(contentEditableVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        focusNode(contentEditableVirtualViewId);

        // Send an end of test signal to ensure test page has fully started since some bots
        // seem to flake when the page has not fully loaded before testing begins.
        mActivityTestRule.sendEndOfTestSignal();

        // Set granularity to CHARACTER, with selection TRUE
        Bundle args = new Bundle();
        args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT,
                AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER);
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping right (forward) (adds to selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping left (backward) (removes from selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(0, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Turn selection mode off and traverse to end so we can select backwards
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, false);
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);
        }

        // Turn selection mode on
        args.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, true);

        // Simulate swiping left (backward) (adds to selections)
        for (int i = 7; i > 0; i--) {
            performTextActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i - 1, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i - 1, mTestData.getSelectionToIndex());
        }

        // Simulate swiping right (forward) (removes from selection)
        for (int i = 0; i < 7; i++) {
            performTextActionOnUiThread(contentEditableVirtualViewId,
                    AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY, args);

            Assert.assertEquals(i, mTestData.getTraverseFromIndex());
            Assert.assertEquals(i + 1, mTestData.getTraverseToIndex());
            Assert.assertEquals(7, mTestData.getSelectionFromIndex());
            Assert.assertEquals(i + 1, mTestData.getSelectionToIndex());
        }
    }

    /**
     * Test |AccessibilityNodeInfo| object for contenteditable node.
     */
    @Test
    @SmallTest
    public void testNodeInfo_className_contenteditable() {
        setupTestWithHTML("<div contenteditable>Edit This</div>");

        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);

        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertTrue(CONTENTEDITABLE_ERROR, mNodeInfo.isEditable());
        Assert.assertEquals(CONTENTEDITABLE_ERROR, "Edit This", mNodeInfo.getText().toString());
    }

    /**
     * Test |AccessibilityNodeInfo| object for node with aria-invalid="true".
     */
    @Test
    @SmallTest
    public void testNodeInfo_errorMessage_true() {
        setupTestWithHTML("<input type='text' aria-invalid='true' value='123456789'>");

        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);

        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertTrue(ARIA_INVALID_ERROR, mNodeInfo.isContentInvalid());
        Assert.assertEquals(ARIA_INVALID_ERROR, "Invalid entry", mNodeInfo.getError());
    }

    /**
     * Test |AccessibilityNodeInfo| object for node with aria-invalid="spelling".
     */
    @Test
    @SmallTest
    public void testNodeInfo_errorMessage_spelling() {
        setupTestWithHTML("<input type='text' aria-invalid='spelling' value='123456789'>");

        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);

        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertTrue(ARIA_INVALID_ERROR, mNodeInfo.isContentInvalid());
        Assert.assertEquals(ARIA_INVALID_ERROR, "Invalid spelling", mNodeInfo.getError());
    }

    /**
     * Test |AccessibilityNodeInfo| object for node with aria-invalid="grammar".
     */
    @Test
    @SmallTest
    public void testNodeInfo_errorMessage_grammar() {
        setupTestWithHTML("<input type='text' aria-invalid='grammar' value='123456789'>");

        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);

        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertTrue(ARIA_INVALID_ERROR, mNodeInfo.isContentInvalid());
        Assert.assertEquals(ARIA_INVALID_ERROR, "Invalid grammar", mNodeInfo.getError());
    }

    /**
     * Test |AccessibilityNodeInfo| object for node with no aria-invalid.
     */
    @Test
    @SmallTest
    public void testNodeInfo_errorMessage_none() {
        setupTestWithHTML("<input type='text'>");

        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);

        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertFalse(ARIA_INVALID_ERROR, mNodeInfo.isContentInvalid());
        Assert.assertNull(ARIA_INVALID_ERROR, mNodeInfo.getError());
    }

    /**
     * Test |AccessibilityNodeInfo| object for node with spelling error, and ensure the
     * spelling error is encoded as a Spannable.
     **/
    @Test
    @SmallTest
    public void testNodeInfo_spellingError() {
        setupTestWithHTML("<input type='text' value='one wordd has an error'>");

        // Call a test API to explicitly add a spelling error in the same format as
        // would be generated if spelling correction was enabled. Clear our cache for this node.
        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mActivityTestRule.mWcax.addSpellingErrorForTesting(textNodeVirtualViewId, 4, 9);
        mActivityTestRule.mWcax.clearNodeInfoCacheForGivenId(textNodeVirtualViewId);

        // Get |AccessibilityNodeInfo| object and confirm it is not null.
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Assert that the node's text has a SuggestionSpan surrounding the proper word.
        CharSequence text = mNodeInfo.getText();
        Assert.assertTrue(SPELLING_ERROR, text instanceof Spannable);

        Spannable spannable = (Spannable) text;
        SuggestionSpan[] spans = spannable.getSpans(0, text.length(), SuggestionSpan.class);
        Assert.assertNotNull(SPELLING_ERROR, spans);
        Assert.assertEquals(SPELLING_ERROR, 1, spans.length);
        Assert.assertEquals(SPELLING_ERROR, 4, spannable.getSpanStart(spans[0]));
        Assert.assertEquals(SPELLING_ERROR, 9, spannable.getSpanEnd(spans[0]));
    }

    /**
     * Test |AccessibilityNodeInfo| object for character bounds for a node in Android O.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @TargetApi(Build.VERSION_CODES.O)
    public void testNodeInfo_extraDataAdded() {
        setupTestWithHTML("<h1>Simple test page</h1><section><p>Text</p></section>");

        // Wait until we find a node in the accessibility tree with the text "Text".
        int textNodeVirtualViewId = waitForNodeMatching(sTextMatcher, "Text");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Call the API we want to test - addExtraDataToAccessibilityNodeInfo.
        final Bundle arguments = new Bundle();
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_START_INDEX, 0);
        arguments.putInt(EXTRA_DATA_TEXT_CHARACTER_LOCATION_ARG_LENGTH, 4);

        // addExtraDataToAccessibilityNodeInfo() will end up calling RenderFrameHostImpl's method
        // AccessibilityPerformAction() in the C++ code, which needs to be run from the UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                    textNodeVirtualViewId, mNodeInfo, EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY,
                    arguments);
        });

        // It should return a result, but all of the rects will be the same because it hasn't
        // loaded inline text boxes yet.
        Bundle extras = mNodeInfo.getExtras();
        RectF[] result =
                (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
        Assert.assertNotEquals(result, null);
        Assert.assertEquals(result.length, 4);
        Assert.assertEquals(result[0], result[1]);
        Assert.assertEquals(result[0], result[2]);
        Assert.assertEquals(result[0], result[3]);

        // The role string should be a camel cased programmatic identifier.
        CharSequence roleString = extras.getCharSequence(EXTRAS_KEY_CHROME_ROLE);
        Assert.assertEquals("paragraph", roleString.toString());

        // The data needed for text character locations loads asynchronously. Block until
        // it successfully returns the character bounds.
        CriteriaHelper.pollUiThread(() -> {
            AccessibilityNodeInfo textNode = createAccessibilityNodeInfo(textNodeVirtualViewId);
            mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                    textNodeVirtualViewId, textNode, EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY,
                    arguments);
            Bundle textNodeExtras = textNode.getExtras();
            RectF[] textNodeResults = (RectF[]) textNodeExtras.getParcelableArray(
                    EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
            Criteria.checkThat(textNodeResults, Matchers.arrayWithSize(4));
            Criteria.checkThat(textNodeResults[0], Matchers.not(textNodeResults[1]));
        });

        // The final result should be the separate bounding box of all four characters.
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.mNodeProvider.addExtraDataToAccessibilityNodeInfo(
                    textNodeVirtualViewId, mNodeInfo, EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY,
                    arguments);
        });

        extras = mNodeInfo.getExtras();
        result = (RectF[]) extras.getParcelableArray(EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY);
        Assert.assertNotEquals(result[0], result[1]);
        Assert.assertNotEquals(result[0], result[2]);
        Assert.assertNotEquals(result[0], result[3]);

        // All four should have nonzero left, top, width, and height
        for (int i = 0; i < 4; ++i) {
            Assert.assertTrue(result[i].left > 0);
            Assert.assertTrue(result[i].top > 0);
            Assert.assertTrue(result[i].width() > 0);
            Assert.assertTrue(result[i].height() > 0);
        }

        // They should be in order.
        Assert.assertTrue(result[0].left < result[1].left);
        Assert.assertTrue(result[1].left < result[2].left);
        Assert.assertTrue(result[2].left < result[3].left);
    }

    @Test
    @SmallTest
    public void testNodeInfo_extras_unclippedBounds() throws Throwable {
        // Build a simple web page with a scrollable view.
        setupTestFromFile("content/test/data/android/scroll_element_offscreen.html");

        // Find the <div> that contains example paragraphs that can be scrolled.
        int vvIdDiv = waitForNodeMatching(sViewIdResourceNameMatcher, "scroll_view");
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // The page may take a moment to finish onload method, so poll for a child count.
        CriteriaHelper.pollUiThread(() -> {
            return createAccessibilityNodeInfo(vvIdDiv).getChildCount() == 100;
        }, NODE_TIMEOUT_ERROR);

        // Focus the scroll container.
        focusNode(vvIdDiv);

        // Send a scroll event so some elements will be offscreen and poll for results.
        performActionOnUiThread(vvIdDiv, WebContentsAccessibilityImpl.ACTION_PAGE_UP, null, () -> {
            return createAccessibilityNodeInfo(vvIdDiv).getExtras() != null
                    && createAccessibilityNodeInfo(vvIdDiv).getExtras().getInt(
                               EXTRAS_KEY_UNCLIPPED_TOP, 1)
                    < 0;
        });

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Refresh the AccessibilityNodeInfo object for the container.
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);

        // Check that the container has unclipped values set.
        Assert.assertNotNull(NODE_EXTRAS_UNCLIPPED_ERROR, mNodeInfo.getExtras());
        Assert.assertTrue(NODE_EXTRAS_UNCLIPPED_ERROR,
                mNodeInfo.getExtras().getInt(EXTRAS_KEY_UNCLIPPED_TOP) < 0);
        Assert.assertTrue(NODE_EXTRAS_UNCLIPPED_ERROR,
                mNodeInfo.getExtras().getInt(EXTRAS_KEY_UNCLIPPED_BOTTOM) > 0);
    }

    /**
     * Test |AccessibilityNodeInfo| object actions to ensure we are not adding ACTION_LONG_CLICK
     * to nodes due to verbose utterances issue.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testNodeInfo_noLongClickAction() {
        // Build a simple web page with a node.
        setupTestWithHTML("<p>Example paragraph</p>");

        int textViewId = waitForNodeMatching(sTextOrContentDescriptionMatcher, "Example paragraph");
        mNodeInfo = createAccessibilityNodeInfo(textViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Confirm the ACTION_LONG_CLICK action has not been added to the node.
        Assert.assertFalse(LONG_CLICK_ERROR, mNodeInfo.getActionList().contains(ACTION_LONG_CLICK));
    }

    /**
     * Test |AccessibilityNodeInfo| object actions for text node.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testNodeInfo_Actions_SetText() {
        // Load a web page with a text field.
        setupTestWithHTML("<input type='text'>");

        int textNodeVirtualViewId =
                waitForNodeMatching(sClassNameMatcher, "android.widget.EditText");
        mNodeInfo = createAccessibilityNodeInfo(textNodeVirtualViewId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Confirm the ACTION_SET_TEXT action has been added to the node.
        Assert.assertTrue(ACTION_SET_ERROR, mNodeInfo.getActionList().contains(ACTION_SET_TEXT));
    }

    /**
     * Test |AccessibilityNodeInfo| object actions for node is specifically user scrollable,
     * and not just programmatically scrollable.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testNodeInfo_Actions_OverflowHidden() throws Throwable {
        // Build a simple web page with a div and overflow:hidden
        setupTestWithHTML("<div title='1234' style='overflow:hidden; width: 200px; height:50px'>\n"
                + "  <p>Example Paragraph 1</p>\n"
                + "  <p>Example Paragraph 2</p>\n"
                + "</div>");

        // Define our root node and paragraph node IDs by looking for their text.
        int vvIdDiv = waitForNodeMatching(sTextMatcher, "1234");
        int vvIdP1 = waitForNodeMatching(sTextMatcher, "Example Paragraph 1");
        int vvIdP2 = waitForNodeMatching(sTextMatcher, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfo nodeInfoDiv = createAccessibilityNodeInfo(vvIdDiv);
        AccessibilityNodeInfo nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        AccessibilityNodeInfo nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP2);

        // Assert the scroll actions are not present in any of the objects.
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Traverse to the next node, then re-assert.
        performActionOnUiThread(vvIdDiv, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Repeat.
        performActionOnUiThread(vvIdP1, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        assertActionsContainNoScrolls(nodeInfoDiv);
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);
    }

    /**
     * Test |AccessibilityNodeInfo| object actions for node is user scrollable.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testNodeInfo_Actions_OverflowScroll() throws Throwable {
        // Build a simple web page with a div and overflow:scroll
        setupTestWithHTML("<div title='1234' style='overflow:scroll; width: 200px; height:50px'>\n"
                + "  <p>Example Paragraph 1</p>\n"
                + "  <p>Example Paragraph 2</p>\n"
                + "</div>");

        // Define our root node and paragraph node IDs by looking for their text.
        int vvIdDiv = waitForNodeMatching(sTextMatcher, "1234");
        int vvIdP1 = waitForNodeMatching(sTextMatcher, "Example Paragraph 1");
        int vvIdP2 = waitForNodeMatching(sTextMatcher, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfo nodeInfoDiv = createAccessibilityNodeInfo(vvIdDiv);
        AccessibilityNodeInfo nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        AccessibilityNodeInfo nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP2);

        // Assert the scroll actions ARE present for our div node, but not the others.
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Traverse to the next node, then re-assert.
        performActionOnUiThread(vvIdDiv, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);

        // Repeat.
        performActionOnUiThread(vvIdP1, ACTION_NEXT_HTML_ELEMENT, new Bundle());
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertTrue(nodeInfoDiv.getActionList().contains(ACTION_SCROLL_DOWN));
        assertActionsContainNoScrolls(nodeInfoP1);
        assertActionsContainNoScrolls(nodeInfoP2);
    }

    /**
     * Test our internal cache of |AccessibilityNodeInfo| objects for proper focus/action updates.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testNodeInfoCache_AccessibilityFocusAndActions() throws Throwable {
        // Build a simple web page with two paragraphs that can be focused.
        setupTestWithHTML("<div>\n"
                + "  <p>Example Paragraph 1</p>\n"
                + "  <p>Example Paragraph 2</p>\n"
                + "</div>");

        // Define our root node and paragraph node IDs by looking for their text.
        int vvIdP1 = waitForNodeMatching(sTextMatcher, "Example Paragraph 1");
        int vvIdP2 = waitForNodeMatching(sTextMatcher, "Example Paragraph 2");

        // Get the |AccessibilityNodeInfo| objects for our nodes.
        AccessibilityNodeInfo nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        AccessibilityNodeInfo nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);

        // Assert we have the correct nodes.
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, nodeInfoP2);

        // Assert neither node has been focused, and both have a accessibility focusable action.
        Assert.assertFalse(nodeInfoP1.isAccessibilityFocused());
        Assert.assertFalse(nodeInfoP2.isAccessibilityFocused());
        Assert.assertTrue(nodeInfoP1.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP1.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP2.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP2.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));

        // Now focus each paragraph in turn and check available actions.
        focusNode(vvIdP1);
        nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);
        Assert.assertTrue(nodeInfoP1.isAccessibilityFocused());
        Assert.assertFalse(nodeInfoP1.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP1.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP2.isAccessibilityFocused());
        Assert.assertTrue(nodeInfoP2.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP2.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));

        // Focus second paragraph to confirm proper cache updates.
        focusNode(vvIdP2);
        nodeInfoP1 = createAccessibilityNodeInfo(vvIdP1);
        nodeInfoP2 = createAccessibilityNodeInfo(vvIdP2);
        Assert.assertFalse(nodeInfoP1.isAccessibilityFocused());
        Assert.assertTrue(nodeInfoP1.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertFalse(nodeInfoP1.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP2.isAccessibilityFocused());
        Assert.assertFalse(nodeInfoP2.getActionList().contains(ACTION_ACCESSIBILITY_FOCUS));
        Assert.assertTrue(nodeInfoP2.getActionList().contains(ACTION_CLEAR_ACCESSIBILITY_FOCUS));
    }

    /**
     * Test our internal cache of |AccessibilityNodeInfo| objects for proper leaf node updates.
     */
    @Test
    @SmallTest
    public void testNodeInfoCache_LeafNodeText() throws Throwable {
        // Build a simple web page with a text node inside a leaf node.
        setupTestFromFile("content/test/data/android/leaf_node_updates.html");

        // Find the encompassing <div> node.
        int vvIdDiv = waitForNodeMatching(sViewIdResourceNameMatcher, "test");
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Example text 1", mNodeInfo.getText());

        // Focus the encompassing node.
        focusNode(vvIdDiv);

        // Run JS code to update the text.
        executeJS("updateText()");

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Check whether the text of the encompassing node has been updated.
        mNodeInfo = createAccessibilityNodeInfo(vvIdDiv);
        Assert.assertEquals(CACHING_ERROR, "Example text 2", mNodeInfo.getText());
    }

    /**
     * Test logic for converting event type masks to a list of relevant event types.
     */
    @Test
    @SmallTest
    public void testMaskToEventTypeConversion() {
        // Build a simple web page.
        setupTestWithHTML("<p>Test page</p>");

        // Create some event masks with known outcomes.
        int serviceEventMask_empty = 0;
        int serviceEventMask_full = Integer.MAX_VALUE;
        int serviceEventMask_test = AccessibilityEvent.TYPE_VIEW_CLICKED
                | AccessibilityEvent.TYPE_VIEW_LONG_CLICKED | AccessibilityEvent.TYPE_VIEW_FOCUSED
                | AccessibilityEvent.TYPE_VIEW_SCROLLED | AccessibilityEvent.TYPE_VIEW_SELECTED
                | AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END;

        // Convert each mask to a set of eventTypes.
        Set<Integer> outcome_empty =
                mActivityTestRule.mWcax.convertMaskToEventTypes(serviceEventMask_empty);
        Set<Integer> outcome_full =
                mActivityTestRule.mWcax.convertMaskToEventTypes(serviceEventMask_full);
        Set<Integer> outcome_test =
                mActivityTestRule.mWcax.convertMaskToEventTypes(serviceEventMask_test);

        // Verify results.
        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_empty);
        Assert.assertTrue(EVENT_TYPE_MASK_ERROR, outcome_empty.isEmpty());

        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_full);
        Assert.assertEquals(EVENT_TYPE_MASK_ERROR, 31, outcome_full.size());

        Set<Integer> expected_test = new HashSet<Integer>(Arrays.asList(
                AccessibilityEvent.TYPE_VIEW_CLICKED, AccessibilityEvent.TYPE_VIEW_LONG_CLICKED,
                AccessibilityEvent.TYPE_VIEW_FOCUSED, AccessibilityEvent.TYPE_VIEW_SCROLLED,
                AccessibilityEvent.TYPE_VIEW_SELECTED,
                AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END));

        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_test);
        Assert.assertEquals(EVENT_TYPE_MASK_ERROR, expected_test, outcome_test);
    }

    /**
     * Test that changing the window size properly updates bounding boxes.
     */
    @Test
    @SmallTest
    public void testBoundingBoxUpdatesOnWindowResize() {
        // Build a simple web page with a flex and a will-change: transform button.
        setupTestWithHTML("<div style=\"display: flex; min-height: 90vh;\">\n"
                + " <div style=\"display: flex; flex-grow: 1; align-items: flex-end;\">\n"
                + "   <div>\n"
                + "     <button style=\"display: inline-flex; will-change: transform;\">\n"
                + "       Next\n"
                + "     </button>\n"
                + "   </div>\n"
                + " </div>\n"
                + "</div>");

        // Find the button and get the current bounding box.
        int buttonvvId = waitForNodeMatching(sClassNameMatcher, "android.widget.Button");
        mNodeInfo = createAccessibilityNodeInfo(buttonvvId);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        Assert.assertEquals(NODE_TIMEOUT_ERROR, "Next", mNodeInfo.getText());

        Rect beforeBounds = new Rect();
        mNodeInfo.getBoundsInScreen(beforeBounds);

        // Resize the web contents.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebContents().setSize(1080, beforeBounds.top / 3));

        // Send end of test signal.
        mActivityTestRule.sendEndOfTestSignal();

        // Fetch the bounding box again and assert top has shrunk by at least half.
        mNodeInfo = createAccessibilityNodeInfo(buttonvvId);
        Rect afterBounds = new Rect();
        mNodeInfo.getBoundsInScreen(afterBounds);

        Assert.assertTrue(BOUNDING_BOX_ERROR, afterBounds.top < (beforeBounds.top / 2));
    }

    /**
     * Test that UMA histograms are recorded for the OnDemand AT feature.
     */
    @Test
    @SmallTest
    public void testOnDemandAccessibilityEventsUMARecorded() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTML("<p>This is a test 1</p>\n"
                + "<p>This is a test 2</p>\n"
                + "<p>This is a test 3</p>");

        // Find the three text nodes.
        int vvId1 = waitForNodeMatching(sTextMatcher, "This is a test 1");
        int vvId2 = waitForNodeMatching(sTextMatcher, "This is a test 2");
        int vvId3 = waitForNodeMatching(sTextMatcher, "This is a test 3");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvId1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvId2);
        AccessibilityNodeInfo mNodeInfo3 = createAccessibilityNodeInfo(vvId3);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo3);

        // Focus each node in turn to generate events.
        focusNode(vvId1);
        focusNode(vvId2);
        focusNode(vvId3);

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Force recording of UMA histograms.
        mActivityTestRule.mWcax.forceRecordUMAHistogramsForTesting();

        // Verify results were recorded in histograms.
        Assert.assertEquals(ONDEMAND_HISTOGRAM_ERROR, 1,
                RecordHistogram.getHistogramTotalCountForTesting(PERCENTAGE_DROPPED_HISTOGRAM));
        Assert.assertEquals(ONDEMAND_HISTOGRAM_ERROR, 1,
                RecordHistogram.getHistogramTotalCountForTesting(EVENTS_DROPPED_HISTOGRAM));
        Assert.assertEquals(ONDEMAND_HISTOGRAM_ERROR, 0,
                RecordHistogram.getHistogramTotalCountForTesting(ONE_HUNDRED_PERCENT_HISTOGRAM));
    }

    /**
     * Test that UMA histogram for 100% events dropped is recorded for the OnDemand AT feature.
     */
    @Test
    @SmallTest
    public void testOnDemandAccessibilityEventsUMARecorded_100Percent() throws Throwable {
        // Build a simple web page with a few nodes to traverse.
        setupTestWithHTML("<p>This is a test 1</p>\n"
                + "<p>This is a test 2</p>\n"
                + "<p>This is a test 3</p>");

        // Set the relevant events type masks to be empty so no events are dispatched.
        mActivityTestRule.mWcax.setEventTypeMaskEmptyForTesting();

        // Find the three text nodes.
        int vvId1 = waitForNodeMatching(sTextMatcher, "This is a test 1");
        int vvId2 = waitForNodeMatching(sTextMatcher, "This is a test 2");
        int vvId3 = waitForNodeMatching(sTextMatcher, "This is a test 3");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvId1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvId2);
        AccessibilityNodeInfo mNodeInfo3 = createAccessibilityNodeInfo(vvId3);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo3);

        // Focus each node in turn to generate events.
        focusNode(vvId1);
        focusNode(vvId2);
        focusNode(vvId3);

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Force recording of UMA histograms.
        mActivityTestRule.mWcax.forceRecordUMAHistogramsForTesting();

        // Verify results were recorded in histograms.
        Assert.assertEquals(ONDEMAND_HISTOGRAM_ERROR, 1,
                RecordHistogram.getHistogramTotalCountForTesting(PERCENTAGE_DROPPED_HISTOGRAM));
        Assert.assertEquals(ONDEMAND_HISTOGRAM_ERROR, 1,
                RecordHistogram.getHistogramTotalCountForTesting(EVENTS_DROPPED_HISTOGRAM));
        Assert.assertEquals(ONDEMAND_HISTOGRAM_ERROR, 1,
                RecordHistogram.getHistogramTotalCountForTesting(ONE_HUNDRED_PERCENT_HISTOGRAM));
    }

    /**
     * Test that isVisibleToUser and offscreen extra are properly reflecting obscured views.
     */
    @Test
    @SmallTest
    public void testNodeInfo_isVisibleToUser_offscreenCSS() {
        // Build a simple web page with nodes that are clipped by CSS.
        setupTestFromFile("content/test/data/android/hide_visible_elements_with_css.html");

        // Find relevant nodes in the list.
        int vvIdText1 = waitForNodeMatching(sTextMatcher, "1");
        int vvIdText2 = waitForNodeMatching(sTextMatcher, "6");
        int vvIdText3 = waitForNodeMatching(sTextMatcher, "9");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvIdText1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvIdText2);
        AccessibilityNodeInfo mNodeInfo3 = createAccessibilityNodeInfo(vvIdText3);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo3);

        // Signal end of test.
        mActivityTestRule.sendEndOfTestSignal();

        // Check visibility of each element, all text nodes should be visible.
        Assert.assertTrue(VISIBLE_TO_USER_ERROR, mNodeInfo1.isVisibleToUser());
        Assert.assertTrue(VISIBLE_TO_USER_ERROR, mNodeInfo2.isVisibleToUser());
        Assert.assertTrue(VISIBLE_TO_USER_ERROR, mNodeInfo3.isVisibleToUser());

        // Check for offscreen Bundle extra, the second two texts should contain.
        Assert.assertFalse(OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo1.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo2.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo2.getExtras().getBoolean(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo3.getExtras().containsKey(EXTRAS_KEY_OFFSCREEN));
        Assert.assertTrue(OFFSCREEN_BUNDLE_EXTRA_ERROR,
                mNodeInfo3.getExtras().getBoolean(EXTRAS_KEY_OFFSCREEN));
    }

    /**
     * Test that the performAction for ACTION_SET_TEXT works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_setText() throws Throwable {
        // Build a simple web page with an input node to interact with.
        setupTestWithHTML("<input type='text'><button>Button</button>");

        // Find input node and button node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        int vvidButton = waitForNodeMatching(sTextMatcher, "Button");
        AccessibilityNodeInfo buttonNodeInfo = createAccessibilityNodeInfo(vvidButton);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, buttonNodeInfo);

        // Verify that bad requests have no effect.
        Assert.assertFalse(
                performActionOnUiThread(vvidButton, AccessibilityNodeInfo.ACTION_SET_TEXT, null));
        Assert.assertFalse(
                performActionOnUiThread(vvid, AccessibilityNodeInfo.ACTION_SET_TEXT, null));
        Bundle bundle = new Bundle();
        bundle.putCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, null);
        Assert.assertFalse(
                performActionOnUiThread(vvid, AccessibilityNodeInfo.ACTION_SET_TEXT, bundle));

        // Send a proper action and poll for update.
        bundle.putCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, "new text");
        Assert.assertTrue(performActionOnUiThread(vvid, AccessibilityNodeInfo.ACTION_SET_TEXT,
                bundle, () -> !createAccessibilityNodeInfo(vvid).getText().toString().isEmpty()));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "new text", mNodeInfo.getText().toString());
    }

    /**
     * Test that the performAction for ACTION_SET_SELECTION works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_setSelection() throws Throwable {
        // Build a simple web page with an input node to interact with.
        setupTestWithHTML("<input type='text' value='test text'><button>Button</button>");

        // Find input node and button node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);
        int vvidButton = waitForNodeMatching(sTextMatcher, "Button");
        AccessibilityNodeInfo buttonNodeInfo = createAccessibilityNodeInfo(vvidButton);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, buttonNodeInfo);

        // Verify that a bad request has no effect.
        Assert.assertFalse(performActionOnUiThread(
                vvidButton, AccessibilityNodeInfo.ACTION_SET_SELECTION, null));

        // Send a proper action and poll for update.
        Bundle bundle = new Bundle();
        bundle.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, 2);
        bundle.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, 5);
        Assert.assertTrue(performActionOnUiThread(
                vvid, AccessibilityNodeInfo.ACTION_SET_SELECTION, bundle, () -> {
                    return createAccessibilityNodeInfo(vvid).getTextSelectionStart() > 0
                            && createAccessibilityNodeInfo(vvid).getTextSelectionEnd() > 0;
                }));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 2, mNodeInfo.getTextSelectionStart());
        Assert.assertEquals(PERFORM_ACTION_ERROR, 5, mNodeInfo.getTextSelectionEnd());
    }

    /**
     * Test that the performAction for ACTION_CUT works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_cut() throws Throwable {
        // Build a simple web page with an input field.
        setupTestWithHTML("<input type='text' value='test text'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Select a given portion of the text.
        Bundle bundle = new Bundle();
        bundle.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, 2);
        bundle.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, 7);
        Assert.assertTrue(performActionOnUiThread(
                vvid, AccessibilityNodeInfo.ACTION_SET_SELECTION, bundle, () -> {
                    return createAccessibilityNodeInfo(vvid).getTextSelectionStart() > 0
                            && createAccessibilityNodeInfo(vvid).getTextSelectionEnd() > 0;
                }));

        // Perform the "cut" action, and poll for clipboard to be non-null.
        ClipboardManager clipboardManager = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                    CLIPBOARD_SERVICE);
        });
        Assert.assertTrue(performActionOnUiThread(
                vvid, ACTION_CUT, null, () -> clipboardManager.getPrimaryClip() != null));

        // Send end of test signal and refresh input node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify text has been properly added to the clipboard.
        Assert.assertNotNull(PERFORM_ACTION_ERROR, clipboardManager.getPrimaryClip());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR, 1, clipboardManager.getPrimaryClip().getItemCount());
        Assert.assertEquals(PERFORM_ACTION_ERROR, "st te",
                clipboardManager.getPrimaryClip().getItemAt(0).getText().toString());

        // Verify input node was changed by the cut action.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "text", mNodeInfo.getText().toString());
    }

    /**
     * Test that the performAction for ACTION_COPY works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_copy() throws Throwable {
        // Build a simple web page with an input field.
        setupTestWithHTML("<input type='text' value='test text'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Select a given portion of the text.
        Bundle bundle = new Bundle();
        bundle.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, 2);
        bundle.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, 7);
        Assert.assertTrue(performActionOnUiThread(
                vvid, AccessibilityNodeInfo.ACTION_SET_SELECTION, bundle, () -> {
                    return createAccessibilityNodeInfo(vvid).getTextSelectionStart() > 0
                            && createAccessibilityNodeInfo(vvid).getTextSelectionEnd() > 0;
                }));

        // Perform the "copy" action, and poll for clipboard to be non-null.
        ClipboardManager clipboardManager = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                    CLIPBOARD_SERVICE);
        });
        Assert.assertTrue(performActionOnUiThread(
                vvid, ACTION_COPY, null, () -> clipboardManager.getPrimaryClip() != null));

        // Send end of test signal and refresh input node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify text has been properly added to the clipboard.
        Assert.assertNotNull(PERFORM_ACTION_ERROR, clipboardManager.getPrimaryClip());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR, 1, clipboardManager.getPrimaryClip().getItemCount());
        Assert.assertEquals(PERFORM_ACTION_ERROR, "st te",
                clipboardManager.getPrimaryClip().getItemAt(0).getText().toString());

        // Verify input node was not changed by the copy action.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "test text", mNodeInfo.getText().toString());
    }

    /**
     * Test that the performAction for ACTION_PASTE works properly with accessibility.
     */
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testPerformAction_paste() throws Throwable {
        // Build a simple web page with an input field.
        setupTestWithHTML("<input type='text'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sInputTypeMatcher, InputType.TYPE_CLASS_TEXT);
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Add some ClipData to the ClipboardManager to paste.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClipboardManager clipboardManager =
                    (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                            CLIPBOARD_SERVICE);
            clipboardManager.setPrimaryClip(ClipData.newPlainText("test text", "test text"));
        });

        // Focus the input field node.
        focusNode(vvid);

        // Perform a paste action and poll for the text to change.
        Assert.assertTrue(performActionOnUiThread(vvid, ACTION_PASTE, null,
                () -> !createAccessibilityNodeInfo(vvid).getText().toString().isEmpty()));

        // Send end of test signal and update node info.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify text has not been removed from the clipboard.
        ClipboardManager clipboardManager =
                (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                        CLIPBOARD_SERVICE);
        Assert.assertNotNull(PERFORM_ACTION_ERROR, clipboardManager.getPrimaryClip());
        Assert.assertEquals(
                PERFORM_ACTION_ERROR, 1, clipboardManager.getPrimaryClip().getItemCount());
        Assert.assertEquals(PERFORM_ACTION_ERROR, "test text",
                clipboardManager.getPrimaryClip().getItemAt(0).getText().toString());

        // Verify text has been properly pasted into the input field.
        Assert.assertEquals(PERFORM_ACTION_ERROR, "test text", mNodeInfo.getText().toString());
    }

    /**
     * Test that the performAction for ACTION_SET_SELECTION works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_setProgress() throws Throwable {
        // Build a simple web page with an element that supports range values.
        setupTestWithHTML("<input id='id1' type='range' min='0' max='50' value='10'>");

        // Find the relevant node.
        int vvid = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        mNodeInfo = createAccessibilityNodeInfo(vvid);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, null));
        Bundle bundle = new Bundle();
        Assert.assertFalse(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, bundle));

        // Send a proper action and poll for update.
        bundle.putFloat(ACTION_ARGUMENT_PROGRESS_VALUE, 20);
        Assert.assertTrue(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, bundle, () -> {
            return Math.abs(createAccessibilityNodeInfo(vvid).getRangeInfo().getCurrent() - 20)
                    < 0.01;
        }));

        // Update node.
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 20, mNodeInfo.getRangeInfo().getCurrent(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getMax(), 0.01);

        // Send action that exceeds max value to test clamping.
        bundle.putFloat(ACTION_ARGUMENT_PROGRESS_VALUE, 55);
        Assert.assertTrue(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, bundle, () -> {
            return Math.abs(createAccessibilityNodeInfo(vvid).getRangeInfo().getCurrent() - 50)
                    < 0.01;
        }));

        // Update node.
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getCurrent(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getMax(), 0.01);

        // Send action that is less than minimum value to test clamping.
        bundle.putFloat(ACTION_ARGUMENT_PROGRESS_VALUE, -5);
        Assert.assertTrue(performActionOnUiThread(vvid, ACTION_SET_PROGRESS, bundle, () -> {
            return Math.abs(createAccessibilityNodeInfo(vvid).getRangeInfo().getCurrent() - 0)
                    < 0.01;
        }));

        // Update node.
        mNodeInfo = createAccessibilityNodeInfo(vvid);

        // Verify results.
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getCurrent(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 0, mNodeInfo.getRangeInfo().getMin(), 0.01);
        Assert.assertEquals(PERFORM_ACTION_ERROR, 50, mNodeInfo.getRangeInfo().getMax(), 0.01);
    }

    /**
     * Test that the performAction for ACTION_SET_SELECTION works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_nextHtmlElement() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Focus our first node.
        focusNode(vvid1);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, null));
        Bundle bundle = new Bundle();
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, null);
        Assert.assertFalse(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, bundle));
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "landmark");
        Assert.assertFalse(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, bundle));

        // Send a proper action and poll for update.
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "p");
        Assert.assertTrue(performActionOnUiThread(vvid1, ACTION_NEXT_HTML_ELEMENT, bundle,
                () -> createAccessibilityNodeInfo(vvid2).isAccessibilityFocused()));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);

        // Verify results.
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /**
     * Test that the performAction for ACTION_SET_SELECTION works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_previousHtmlElement() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Focus our second node.
        focusNode(vvid2);

        // Verify that bad requests have no effect.
        Assert.assertFalse(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, null));
        Bundle bundle = new Bundle();
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, null);
        Assert.assertFalse(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, bundle));
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "landmark");
        Assert.assertFalse(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, bundle));

        // Send a proper action and poll for update.
        bundle.putString(ACTION_ARGUMENT_HTML_ELEMENT_STRING, "p");
        Assert.assertTrue(performActionOnUiThread(vvid2, ACTION_PREVIOUS_HTML_ELEMENT, bundle,
                () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        // Send of test signal and update node.
        mActivityTestRule.sendEndOfTestSignal();
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);

        // Verify results.
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /**
     * Test that the performAction for ACTION_ACCESSIBILITY_FOCUS works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_accessibilityFocus() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(vvid1, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS,
                        null, () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /**
     * Test that the performAction for ACTION_CLEAR_ACCESSIBILITY_FOCUS works properly
     * with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_accessibilityClearFocus() throws Throwable {
        // Build a simple web page with elements that can be traversed.
        setupTestWithHTML("<p id='id1'>Example1</p><p id='id2'>Example2</p>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(
                performActionOnUiThread(vvid1, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS,
                        null, () -> createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());

        // Clear accessibility focus from the node and verify.
        Assert.assertTrue(performActionOnUiThread(vvid1,
                AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS, null,
                () -> !createAccessibilityNodeInfo(vvid1).isAccessibilityFocused()));

        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo1.isAccessibilityFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isAccessibilityFocused());
    }

    /**
     * Test that the performAction for ACTION_FOCUS works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_Focus() throws Throwable {
        // Build a simple web page with elements that can be focused.
        setupTestWithHTML("<input type='text' id='id1'><input type='text' id='id2'>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(performActionOnUiThread(vvid1, AccessibilityNodeInfo.ACTION_FOCUS, null,
                () -> createAccessibilityNodeInfo(vvid1).isFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isFocused());
    }

    /**
     * Test that the performAction for ACTION_CLEAR_FOCUS works properly with accessibility.
     */
    @Test
    @SmallTest
    public void testPerformAction_clearFocus() throws Throwable {
        // Build a simple web page with elements that can be focused.
        setupTestWithHTML("<input type='text' id='id1'><input type='text' id='id2'>");

        // Find the relevant nodes.
        int vvid1 = waitForNodeMatching(sViewIdResourceNameMatcher, "id1");
        int vvid2 = waitForNodeMatching(sViewIdResourceNameMatcher, "id2");
        AccessibilityNodeInfo mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        AccessibilityNodeInfo mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo1);
        Assert.assertNotNull(NODE_TIMEOUT_ERROR, mNodeInfo2);

        // Send an action and poll for update.
        Assert.assertTrue(performActionOnUiThread(vvid1, AccessibilityNodeInfo.ACTION_FOCUS, null,
                () -> createAccessibilityNodeInfo(vvid1).isFocused()));

        // Update nodes and verify results.
        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertTrue(PERFORM_ACTION_ERROR, mNodeInfo1.isFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isFocused());

        // Clear focus from the node and verify.
        Assert.assertTrue(performActionOnUiThread(vvid1, AccessibilityNodeInfo.ACTION_CLEAR_FOCUS,
                null, () -> !createAccessibilityNodeInfo(vvid1).isFocused()));

        mNodeInfo1 = createAccessibilityNodeInfo(vvid1);
        mNodeInfo2 = createAccessibilityNodeInfo(vvid2);
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo1.isFocused());
        Assert.assertFalse(PERFORM_ACTION_ERROR, mNodeInfo2.isFocused());
    }

    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    private void assertActionsContainNoScrolls(AccessibilityNodeInfo nodeInfo) {
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_FORWARD));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_BACKWARD));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_UP));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_DOWN));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_LEFT));
        Assert.assertFalse(nodeInfo.getActionList().contains(ACTION_SCROLL_RIGHT));
    }

    private String thresholdError(int count, int max) {
        return THRESHOLD_ERROR + " Received " + count + ", but expected no more than: " + max;
    }

    private String lowThresholdError(int count) {
        return THRESHOLD_LOW_EVENT_COUNT_ERROR + " Received " + count
                + ", but expected at least: " + UNSUPPRESSED_EXPECTED_COUNT;
    }
}
