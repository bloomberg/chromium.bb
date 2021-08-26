// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.concurrent.ExecutionException;

/**
 * Tests for WebContentsAccessibilityImpl integration with AT.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
@SuppressLint("VisibleForTests")
public class WebContentsAccessibilityEventsTest {
    // Test output error messages.
    private static final String EVENTS_ERROR =
            "Generated events and actions did not match expectations.";
    private static final String EXPECTATIONS_NULL =
            "Test expectations were null, perhaps the file is missing?";
    private static final String RESULTS_NULL =
            "Test results were null, did you remember to add the tracker to WCAI?";

    // Member variables required for testing framework
    private static final String BASE_DIRECTORY = "/chromium_tests_root";
    private static final String BASE_FILE_PATH = "content/test/data/accessibility/event/";
    private static final String EMPTY_EXPECTATIONS_FILE = "EmptyExpectationsFile";

    @Rule
    public AccessibilityContentShellActivityTestRule mActivityTestRule =
            new AccessibilityContentShellActivityTestRule();

    /**
     * Helper methods for setup of a basic web contents accessibility unit test.
     *
     * This method replaces the usual setUp() method annotated with @Before because we wish to
     * load different data with each test, but the process is the same for all tests.
     *
     * Leaving a commented @Before annotation on each method as a reminder/context clue.
     */
    /* @Before */
    protected void setupTestFromFile(String filepath) {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.getIsolatedTestFileUrl(filepath));
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
        mActivityTestRule.setupTestFramework();
        mActivityTestRule.setAccessibilityDelegate();
        mActivityTestRule.sendReadyForTestSignal();
    }

    /**
     * Perform a single test which will:
     *      1. Open the given HTML file
     *      2. Execute the javascript method "go()"
     *      3. Read expectations file and compare with results
     *
     * @param inputFile                     HTML test input file
     * @param expectationFile               TXT expectations file
     */
    private void performTest(String inputFile, String expectationFile) {
        performTestWithJavascriptMethod(inputFile, expectationFile, "go()");
    }

    /**
     * Perform a single test which will:
     *      1. Open the given HTML file
     *      2. Execute the javascript method "go()"
     *      3. Repeat above step a total of |count| times
     *      4. Read expectations file and compare with results
     *
     * @param inputFile                     HTML test input file
     * @param expectationFile               TXT expectations file
     * @param count                         Number of times to run method.
     */
    private void performTestWithRepeatCounter(String inputFile, String expectationFile, int count) {
        // Build page from given file and enable testing framework, set a tracker.
        setupTestFromFile(BASE_FILE_PATH + inputFile);

        // Execute method a given number of times.
        for (int i = 0; i < count; i++) {
            mActivityTestRule.executeJS("go()");
        }

        // Send an "end of test" signal, then check results.
        mActivityTestRule.sendEndOfTestSignal();
        assertResults(expectationFile);
    }

    /**
     * Perform a single test which will:
     *      1. Open the given HTML file
     *      2. Execute the given javascript method
     *      3. Read expectations file and compare with results
     *
     * @param inputFile                     HTML test input file
     * @param expectationFile               TXT expectations file
     * @param javascriptMethod              javascript method (e.g. "expand()" or "go()")
     */
    private void performTestWithJavascriptMethod(
            String inputFile, String expectationFile, String javascriptMethod) {
        // Build page from given file and enable testing framework, set a tracker.
        setupTestFromFile(BASE_FILE_PATH + inputFile);

        // Execute given javascript function.
        executeJS(javascriptMethod);

        // Send an "end of test" signal, then check results.
        mActivityTestRule.sendEndOfTestSignal();
        assertResults(expectationFile);
    }

    /**
     * Helper method to compare test outputs with expected results. Reads content of expectations
     * file, asserts non-null, then compares with results.
     *
     * @param expectationFile           Filename of the expectations for the given test.
     */
    private void assertResults(String expectationFile) {
        String expectedResults;
        // Test specified that the expected outcome should be empty, no file included.
        if (expectationFile.equals(EMPTY_EXPECTATIONS_FILE)) {
            expectedResults = "";
        } else {
            expectedResults = readExpectationFile(expectationFile);
        }
        Assert.assertNotNull(EXPECTATIONS_NULL, expectedResults);

        String actualResults = getTrackerResults();
        Assert.assertNotNull(RESULTS_NULL, actualResults);

        Assert.assertEquals(EVENTS_ERROR + "\n\nExpected:\n" + expectedResults + "\n\nActual:\n"
                        + actualResults + "\n\n",
                expectedResults, actualResults);
    }

    /**
     * Read the contents of a file, and return as a String.
     *
     * @param fileName              Filename to read
     * @return String               Contents of the given file.
     */
    private String readExpectationFile(String fileName) {
        String directory = Environment.getExternalStorageDirectory().getPath() + BASE_DIRECTORY;

        try {
            File expectedFile = new File(directory, "/" + BASE_FILE_PATH + fileName);
            FileInputStream fis = new FileInputStream(expectedFile);

            byte[] data = new byte[(int) expectedFile.length()];
            fis.read(data);
            fis.close();

            return new String(data);
        } catch (IOException e) {
            return null;
        }
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

    private void executeJS(String method) {
        mActivityTestRule.executeJS(method);
    }

    private void focusNode(int virtualViewId) throws Throwable {
        mActivityTestRule.focusNode(virtualViewId);
    }

    private String getTrackerResults() {
        return mActivityTestRule.getTrackerResults();
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1186376")
    public void test_addAlert() {
        performTest("add-alert.html", "add-alert-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_addAlertWithRoleChange() {
        performTest("add-alert-with-role-change.html",
                "add-alert-with-role-change-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_addChild() {
        performTest("add-child.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_addChildOfBody() {
        performTest("add-child-of-body.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void test_addDialog() {
        performTest("add-dialog.html", "add-dialog-expected-android.txt");
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void test_addDialog_describedBy() {
        performTest("add-dialog-described-by.html", "add-dialog-described-by-expected-android.txt");
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void test_addDialog_noInfo() {
        performTest("add-dialog-no-info.html", "add-dialog-no-info-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_addHiddenAttribute() {
        performTest("add-hidden-attribute.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_addHiddenAttributeSubtree() {
        performTest("add-hidden-attribute-subtree.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_addSubtree() {
        performTest("add-subtree.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaAtomicChanged() {
        performTest("aria-atomic-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaAtomicChanged2() {
        performTest("aria-atomic-changed2.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaBusyChanged() {
        performTest("aria-busy-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaButtonExpand() {
        performTest("aria-button-expand.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaCheckedChanged() {
        performTest("aria-checked-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaComboboxCollapse() {
        performTest("aria-combo-box-collapse.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaComboboxDelayAddList() {
        performTest("aria-combo-box-delay-add-list.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaComboboxDelayShowList() {
        performTest("aria-combo-box-delay-show-list.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaComboboxExpand() {
        performTestWithRepeatCounter(
                "aria-combo-box-expand.html", "aria-combo-box-expand-expected-android.txt", 3);
    }

    @Test
    @SmallTest
    public void test_ariaComboboxFocus() {
        performTest("aria-combo-box-focus.html", "aria-combo-box-focus-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_ariaComboboxNext() {
        performTest("aria-combo-box-next.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaComboboxUneditable() {
        performTest("aria-combo-box-uneditable.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaControlsChanged() {
        performTest("aria-controls-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaCurrentChanged() {
        performTest("aria-current-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaDisabledChanged() {
        performTest("aria-disabled-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaDropeffectChanged() {
        performTest("aria-dropeffect-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaFlowTo() {
        performTest("aria-flow-to.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaGrabbedChanged() {
        performTest("aria-grabbed-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHasPopupChanged() {
        performTest("aria-haspopup-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHiddenChanged() {
        performTest("aria-hidden-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHiddenDescendantsAlreadyIgnored() {
        performTest("aria-hidden-descendants-already-ignored.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHiddenDescendants() {
        performTest("aria-hidden-descendants.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHiddenDescendantDisplayNone() {
        performTest("aria-hidden-single-descendant-display-none.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHiddenSingleDescendant() {
        performTest("aria-hidden-single-descendant.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaHiddenSingleDescendantVisibilityHidden() {
        performTest(
                "aria-hidden-single-descendant-visibility-hidden.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaInvalidChanged() {
        performTest("aria-invalid-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaLevelChanged() {
        performTest("aria-level-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaLiveChanged() {
        performTest("aria-live-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_ariaMenuItemFocus() {
        performTest("aria-menuitem-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaMultilineChanged() {
        performTest("aria-multiline-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaMultiselectableChanged() {
        performTest("aria-multiselectable-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaPosinsetChanged() {
        performTest("aria-posinset-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaPressedChanged() {
        performTest("aria-pressed-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaReadonlyChanged() {
        performTest("aria-readonly-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaRelevantChanged() {
        performTest("aria-relevant-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaRelevantChanged2() {
        performTest("aria-relevant-changed2.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaRequiredChanged() {
        performTest("aria-required-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaSelectedChanged() {
        performTest("aria-selected-changed.html", "aria-selected-changed-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_ariaSetsizeChanged() {
        performTest("aria-setsize-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaSliderValueBothChanged() {
        performTest("aria-slider-value-both-change.html",
                "aria-slider-value-both-change-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_ariaSliderValueChanged() {
        performTest(
                "aria-slider-value-change.html", "aria-slider-value-change-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_ariaSliderValuetextChanged() {
        performTest("aria-slider-valuetext-change.html",
                "aria-slider-valuetext-change-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_ariaSortChanged() {
        performTest("aria-sort-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaSpinbuttonValueBothChanged() {
        performTest("aria-spinbutton-value-both-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaSpinbuttonValueChanged() {
        performTest("aria-spinbutton-value-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaSpinbuttonValuetextChanged() {
        performTest("aria-spinbutton-valuetext-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaTextboxChildrenChange() {
        performTest("aria-textbox-children-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaTextboxEditabilityChanges() {
        performTest("aria-textbox-editability-changes.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaTextboxWithFocusableChildren() {
        performTest("aria-textbox-with-focusable-children.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaTreeCollapse() {
        performTest("aria-tree-collapse.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ariaTreeExpand() {
        performTest("aria-tree-expand.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_ariaTreeItemFocus() {
        performTest("aria-treeitem-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_buttonClick() {
        performTest("button-click.html", "button-click-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_buttonRemoveChildren() {
        performTest("button-remove-children.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_caretBrowsingDisabled() {
        performTest("caret-browsing-disabled.html", "caret-browsing-disabled-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_caretBrowsingEnabled() {
        performTest("caret-browsing-enabled.html", "caret-browsing-enabled-expected-android.txt");
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1186376")
    public void test_caretHide() {
        performTest("caret-hide.html", "caret-hide-expected-android.txt");
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1186376")
    public void test_caretMoveHiddenInput() {
        performTest("caret-move-hidden-input.html", "caret-move-hidden-input-expected-android.txt");
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1186376")
    public void test_caretMove() {
        performTest("caret-move.html", "caret-move-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_checkboxValidity() {
        performTest("checkbox-validity.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_checkedMixedChanged() {
        performTest("checked-mixed-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_checkedStateChanged() {
        performTest("checked-state-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_childrenChangedOnlyOnAncestor() {
        performTest("children-changed-only-on-ancestor.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_cssDisplayDescendants() {
        performTest("css-display-descendants.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_cssDisplay() {
        performTest("css-display.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_cssFlexTextUpdate() {
        performTest("css-flex-text-update.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_cssVisibilityCollapse() {
        performTest("css-visibility-collapse.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_cssVisibilityDescendants() {
        performTest("css-visibility-descendants.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_cssVisibility() {
        performTest("css-visibility.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_deleteSubtree() {
        performTest("delete-subtree.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_descriptionChanged() {
        performTest("description-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_descriptionChangedIndirect() {
        performTest("description-change-indirect.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_descriptionChangedNoRelation() {
        performTest("description-change-no-relation.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_disabledStateChanged() {
        performTest("disabled-state-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_documentTitleChanged() {
        performTest("document-title-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_expandedChanged() {
        performTest("expanded-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_focusListbox() {
        performTest("focus-listbox.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_focusListboxMultiselect() {
        performTest("focus-listbox-multiselect.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_formDisabledChanged() {
        performTest("form-disabled-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_formRequiredChanged() {
        performTest("form-required-changed.html", "form-required-changed-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_innerHtmlChanged() {
        performTest("inner-html-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_inputTypeTextValueChanged() {
        performTest("input-type-text-value-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_listboxFocus() {
        performTest("listbox-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_listboxNext() {
        performTest("listbox-next.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_liveRegionAdd() {
        performTest("live-region-add.html", "live-region-add-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_liveRegionAddLiveAttribute() {
        performTest("live-region-add-live-attribute.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_liveRegionChanged() {
        performTest("live-region-change.html", "live-region-change-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_liveRegionChangedInnerHtml() {
        performTest("live-region-change-innerhtml.html",
                "live-region-change-innerhtml-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_liveRegionChangedInnerText() {
        performTest("live-region-change-innertext.html",
                "live-region-change-innertext-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_liveRegionCreate() {
        performTest("live-region-create.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_liveRegionElemReparent() {
        performTest(
                "live-region-elem-reparent.html", "live-region-elem-reparent-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_liveRegionIgnoresClick() {
        performTest(
                "live-region-ignores-click.html", "live-region-ignores-click-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_liveRegionOff() {
        performTest("live-region-off.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_liveRegionRemove() {
        performTest("live-region-remove.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menuBarShowHideMenus() {
        performTest("menubar-show-hide-menus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menulistCollapse() {
        performTest("menulist-collapse.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menulistCollapseNext() {
        performTest("menulist-collapse-next.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menulistExpand() {
        performTest("menulist-expand.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_menulistFocus() {
        performTest("menulist-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menulistNext() {
        performTest("menulist-next.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menulistPopup() {
        performTest("menulist-popup.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_menuOpenedClosed() {
        performTest("menu-opened-closed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_multipleAriaPropertiesChanged() {
        performTest("multiple-aria-properties-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_nameChanged() {
        performTest("name-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_nameChangedIndirect() {
        performTest("name-change-indirect.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_pressedStateChanged() {
        performTest("pressed-state-change.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_rangeValueIsReadonlyChanged() {
        performTest("range-value-is-readonly-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_rangeValueMaximumChanged() {
        performTest("range-value-maximum-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_rangeValueMinimumChanged() {
        performTest("range-value-minimum-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_rangeValueStepChanged() {
        performTest("range-value-step-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_rangeValueValueChanged() {
        performTest(
                "range-value-value-changed.html", "range-value-value-changed-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_removeChild() {
        performTest("remove-child.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_removeHiddenAttribute() {
        performTest("remove-hidden-attribute.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_removeHiddenAttributeSubtree() {
        performTest("remove-hidden-attribute-subtree.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_removeSubtree() {
        performTest("remove-subtree.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_reportValidityInvalidField() {
        performTest("report-validity-invalid-field.html",
                "report-validity-invalid-field-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_samePageLinkNavigation() {
        performTest(
                "same-page-link-navigation.html", "same-page-link-navigation-expected-android.txt");
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1186376")
    public void test_scrollHorizontalScrollPercentChanged() {
        performTest("scroll-horizontal-scroll-percent-change.html",
                "scroll-horizontal-scroll-percent-change-expected-android.txt");
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1186376")
    public void test_scrollVerticalScrollPercentChanged() {
        performTest("scroll-vertical-scroll-percent-change.html",
                "scroll-vertical-scroll-percent-change-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_selectSelectedAddRemove() {
        performTest("select-selected-add-remove.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_styleChanged() {
        performTest("style-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_subtreeReparentedIgnoredChanged() {
        performTest("subtree-reparented-ignored-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_subtreeReparentedViaAppendChild() {
        performTest("subtree-reparented-via-append-child.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_subtreeReparentedViaAriaOwns() {
        performTest("subtree-reparented-via-aria-owns.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_tabIndexAddedOnAriaHidden() {
        performTest("tabindex-added-on-aria-hidden.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_tabIndexAddedOnPlainDiv() {
        performTest("tabindex-added-on-plain-div.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_tabIndexRemoveOnAriaHidden() {
        performTest("tabindex-removed-on-aria-hidden.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_tabIndexRemovedOnPlainDiv() {
        performTest("tabindex-removed-on-plain-div.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_tbodyFocus() {
        performTest("tbody-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_textAlignChanged() {
        performTest("text-align-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_textChangedContenteditable() {
        performTest("text-changed-contenteditable.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_textChanged() {
        performTest("text-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_textIndentChanged() {
        performTest("text-indent-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_textSelectionChanged() {
        performTest("text-selection-changed.html", "text-selection-changed-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_textSelectionInsideHiddenElement() {
        performTest("text-selection-inside-hidden-element.html",
                "text-selection-inside-hidden-element-expected-android.txt");
    }

    @Test
    @SmallTest
    public void test_textSelectionInsideVideo() {
        performTest("text-selection-inside-video.html",
                "text-selection-inside-video-expected-android.txt");
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_tfootFocus() {
        performTest("tfoot-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "https://crbug.com/1190218")
    public void test_theadFocus() {
        performTest("thead-focus.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_valudIsReadonlyChanged() {
        performTest("value-is-readonly-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_ValueValueChanged() {
        performTest("value-value-changed.html", EMPTY_EXPECTATIONS_FILE);
    }

    @Test
    @SmallTest
    public void test_visibilityHiddenChanged() {
        performTest("visibility-hidden-changed.html", EMPTY_EXPECTATIONS_FILE);
    }
}
