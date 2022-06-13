// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, RootPath, sendTestMessage, wait} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

const tooltipQueryHidden = 'files-tooltip:not([visible])';
const tooltipQueryVisible = 'files-tooltip[visible=true]';
const searchButton = '#search-button[has-tooltip]';
const viewButton = '#view-button[has-tooltip]';
const readonlyIndicator =
    '#read-only-indicator[has-tooltip][show-card-tooltip]';
const fileList = '#file-list';
const cancelButton = '#cancel-selection-button[has-tooltip]';
const deleteButton = '#delete-button[has-tooltip]';

const tooltipShowTimeout = 500;  // ms

/**
 * $i18n{} labels used when template replacement is disabled.
 *
 * @const {!Object<string, string>}
 */
const i18nLabelReplacements = {
  'SEARCH_TEXT_LABEL': 'Search',
  'READONLY_INDICATOR_TOOLTIP': 'The contents of this folder are read-only. ' +
      'Some activities are not supported.',
  'CANCEL_SELECTION_BUTTON_LABEL': 'Cancel selection',
};

/**
 * Returns $i18n{} label if devtools code coverage is enabled, otherwise the
 * replaced contents.
 *
 * @param {string} key $i18n{} key of replacement text
 * @return {!Promise<string>}
 */
async function getExpectedLabelText(key) {
  const isDevtoolsCoverageActive =
      await sendTestMessage({name: 'isDevtoolsCoverageActive'});

  if (isDevtoolsCoverageActive === 'true') {
    return '$i18n{' + key + '}';
  }

  // Verify |key| has a $i18n{} replacement in |i18nLabelReplacements|.
  const label = i18nLabelReplacements[key];
  chrome.test.assertEq('string', typeof label, 'Missing: ' + key);

  return label;
}

/**
 * Waits until the element by |id| is the document.activeElement.
 *
 * @param {string} appId The Files app windowId.
 * @param {string} id The element id.
 * @return {!Promise}
 */
function getActiveElementById(appId, id) {
  const caller = getCaller();
  return repeatUntil(async () => {
    const element =
        await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
    if (!element || element.attributes['id'] !== id) {
      return pending(caller, 'Waiting for active element by id #%s.', id);
    }
  });
}

/**
 * Tests that tooltip is displayed when focusing an element with tooltip.
 */
testcase.filesTooltipFocus = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Focus a button with a tooltip: the search button.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, [searchButton]));
  await getActiveElementById(appId, 'search-button');

  // Check: the search button tooltip should be visible.
  let expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
  let label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, label.text);

  // Focus an element that has no tooltip: the file-list.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, [fileList]));
  await getActiveElementById(appId, 'file-list');

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Select all the files to enable the cancel selection button.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

  // Focus the cancel selection button.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, [cancelButton]));
  await getActiveElementById(appId, 'cancel-selection-button');

  // Check: the cancel selection button tooltip should be visible.
  expectedLabelText =
      await getExpectedLabelText('CANCEL_SELECTION_BUTTON_LABEL');
  label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, label.text);
};

/**
 * Tests that tooltips display when hovering an element that has a tooltip.
 */
testcase.filesTooltipMouseOver = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Mouse hover over a button that has a tooltip: the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
  const firstElement =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, firstElement.text);

  // Move the mouse away from the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOut', appId, [searchButton]));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Move the mouse over the search button again.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const lastElement =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, lastElement.text);
};

/**
 * Tests that tooltip is hidden when clicking on body (or anything else).
 */
testcase.filesTooltipClickHides = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Hover over a button that has a tooltip: the search button.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [searchButton]));

  // Check: the search button tooltip should be visible.
  const expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
  const label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, label.text);

  // Click the body element.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
};

/**
 * Tests that card tooltip is hidden when clicking on body (or anything else).
 */
testcase.filesCardTooltipClickHides = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Click the 'Android files' volume tab in the directory tree.
  await remoteCall.simulateUiClick(
      appId, ['[volume-type-for-testing=android_files]']);

  // Wait for the read-only bubble to appear in the files app tool bar.
  const readonlyBubbleShown = '#read-only-indicator:not([hidden])';
  await remoteCall.waitForElement(appId, readonlyBubbleShown);

  // Check: initially, no tooltip should be visible.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Hover the mouse over the read-only bubble.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [readonlyIndicator]));

  // Check: the read-only bubble card tooltip should be visible.
  const expectedLabelText =
      await getExpectedLabelText('READONLY_INDICATOR_TOOLTIP');
  const tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
  const label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, label.text);
  chrome.test.assertEq('card-tooltip', tooltip.attributes['class']);
  chrome.test.assertEq('card-label', label.attributes['class']);

  // Click the body element.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
};

/**
 * Tests that the tooltip should hide when the window resizes.
 */
testcase.filesTooltipHidesOnWindowResize = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Focus a button with tooltip: the search button.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('focus', appId, [searchButton]));
  await getActiveElementById(appId, 'search-button');

  // Check: the search button tooltip should be visible.
  const expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
  const label =
      await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
  chrome.test.assertEq(expectedLabelText, label.text);

  // Resize the window.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('resizeWindow', appId, [1200, 1200]));

  // Check: the tooltip should hide.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
};

/**
 * Tests that the tooltip is hidden after the delete confirm dialog closes.
 */
testcase.filesTooltipHidesOnDeleteDialogClosed = async () => {
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], [ENTRIES.beautiful, ENTRIES.photos]);

  const fileListItemQuery = '#file-list li[file-name="Beautiful Song.ogg"]';

  // Check: initially the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Select file.
  await remoteCall.waitAndClickElement(appId, fileListItemQuery);

  // Mouse over the delete button and leave time for tooltip to show.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [deleteButton]));
  await wait(tooltipShowTimeout);

  // Click the toolbar delete button.
  await remoteCall.waitAndClickElement(appId, deleteButton);

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the delete confirm dialog 'Cancel' button.
  const dialogCancelButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-cancel:focus');
  chrome.test.assertEq('Cancel', dialogCancelButton.text);

  // Leave time for tooltip to show.
  await wait(tooltipShowTimeout);

  // Check: the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);

  // Select file.
  await remoteCall.waitAndClickElement(appId, fileListItemQuery);

  // Mouse over the delete button and leave time for tooltip to show.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [deleteButton]));
  await wait(tooltipShowTimeout);

  // Click the toolbar delete button.
  await remoteCall.waitAndClickElement(appId, deleteButton);

  // Check: the delete confirm dialog should appear.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Click the delete confirm dialog 'Delete' button.
  const dialogDeleteButton =
      await remoteCall.waitAndClickElement(appId, '.cr-dialog-ok');
  chrome.test.assertEq('Delete', dialogDeleteButton.text);

  // Check: the delete confirm dialog should close.
  await remoteCall.waitForElementLost(appId, '.cr-dialog-container.shown');

  // Leave time for tooltip to show.
  await wait(tooltipShowTimeout);

  // Check: the tooltip should be hidden.
  await remoteCall.waitForElement(appId, tooltipQueryHidden);
};
