// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AppState} from 'chrome://scanning/scanning_app_types.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible, waitAfterNextRender} from '../../test_util.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

export function scanPreviewTest() {
  /** @type {?ScanPreviewElement} */
  let scanPreview = null;


  /** @type {!HTMLElement} */
  let helpOrProgress;
  /** @type {!HTMLElement} */
  let helperText;
  /** @type {!HTMLElement} */
  let scanProgress;
  /** @type {!HTMLElement} */
  let scannedImages;
  /** @type {!HTMLElement} */
  let cancelingProgress;

  setup(() => {
    scanPreview = /** @type {!ScanPreviewElement} */ (
        document.createElement('scan-preview'));
    assertTrue(!!scanPreview);
    ScanningBrowserProxyImpl.instance_ = new TestScanningBrowserProxy();
    document.body.appendChild(scanPreview);

    helpOrProgress =
        /** @type {!HTMLElement} */ (scanPreview.$$('#helpOrProgress'));
    helperText =
        /** @type {!HTMLElement} */ (scanPreview.$$('#helperText'));
    scanProgress =
        /** @type {!HTMLElement} */ (scanPreview.$$('#scanProgress'));
    scannedImages =
        /** @type {!HTMLElement} */ (scanPreview.$$('#scannedImages'));
    cancelingProgress =
        /** @type {!HTMLElement} */ (scanPreview.$$('#cancelingProgress'));
  });

  teardown(() => {
    if (scanPreview) {
      scanPreview.remove();
    }
    scanPreview = null;
  });

  /**
   * @param {boolean} isHelpOrProgressVisible
   * @param {boolean} isHelperTextVisible
   * @param {boolean} isScanProgressVisible
   * @param {boolean} isScannedImagesVisible
   * @param {boolean} isCancelingProgressVisible
   */
  function assertVisible(
      isHelpOrProgressVisible, isHelperTextVisible, isScanProgressVisible,
      isScannedImagesVisible, isCancelingProgressVisible) {
    assertEquals(isHelpOrProgressVisible, isVisible(helpOrProgress));
    assertEquals(isHelperTextVisible, isVisible(helperText));
    assertEquals(isScanProgressVisible, isVisible(scanProgress));
    assertEquals(isScannedImagesVisible, isVisible(scannedImages));
    assertEquals(isCancelingProgressVisible, isVisible(cancelingProgress));
  }

  test('initializeScanPreview', () => {
    assertTrue(!!scanPreview.$$('.preview'));
  });

  // Test that the progress text updates when the page number increases.
  test('progressTextUpdates', () => {
    scanPreview.appState = AppState.SCANNING;
    scanPreview.pageNumber = 1;
    assertEquals(
        scanPreview.i18n('scanPreviewProgressText', 1),
        scanPreview.$$('#progressText').textContent.trim());
    scanPreview.pageNumber = 2;
    assertEquals(
        scanPreview.i18n('scanPreviewProgressText', 2),
        scanPreview.$$('#progressText').textContent.trim());
  });

  // Tests that the correct element is showing in the preview pane depending on
  // current app state.
  test('appStateTransitions', () => {
    scanPreview.appState = AppState.GETTING_SCANNERS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.GOT_SCANNERS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.GETTING_CAPS;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.READY;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ true,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.SCANNING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ true, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ false);

    scanPreview.appState = AppState.CANCELING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ true);

    scanPreview.appState = AppState.MULTI_PAGE_CANCELING;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ true, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ false,
        /*isCancelingProgressVisible*/ true);

    scanPreview.objectUrls = ['image'];
    scanPreview.appState = AppState.DONE;
    flush();
    assertVisible(
        /*isHelpOrProgressVisible*/ false, /*isHelperTextVisible*/ false,
        /*isScanProgressVisible*/ false, /*isScannedImagesVisible*/ true,
        /*isCancelingProgressVisible*/ false);
  });

  // Tests that the action toolbar is only displayed for multi-page scans.
  test('showActionToolbarForMultiPageScans', () => {
    scanPreview.objectUrls = ['image'];
    scanPreview.appState = AppState.DONE;
    return flushTasks().then(() => {
      assertTrue(scanPreview.$$('action-toolbar').hidden);
      scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;
      flush();
      assertFalse(scanPreview.$$('action-toolbar').hidden);
    });
  });

  // Tests that the toolbar will get repositioned after subsequent scans.
  test('positionActionToolbarOnSubsequentScans', () => {
    const scannedImagesDiv =
        /** @type {!HTMLElement} */ (scanPreview.$$('#scannedImages'));
    scanPreview.objectUrls = [];
    scanPreview.isMultiPageScan = true;
    scanPreview.appState = AppState.MULTI_PAGE_SCANNING;
    return flushTasks()
        .then(() => {
          // Before the image loads we expect the CSS variables to be unset.
          assertEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

          scanPreview.objectUrls = ['svg/ready_to_scan.svg'];
          scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;
          return waitAfterNextRender(scannedImagesDiv);
        })
        .then(() => {
          // After the image loads we expect the CSS variables to be set.
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

          // Reset the CSS variabls and delete the image to simulate starting a
          // new scan.
          scanPreview.style.setProperty('--action-toolbar-top', '');
          scanPreview.style.setProperty('--action-toolbar-left', '');
          assertEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));
          scanPreview.objectUrls = [];
          scanPreview.appState = AppState.MULTI_PAGE_SCANNING;
          return flushTasks();
        })
        .then(() => {
          scanPreview.objectUrls = ['svg/ready_to_scan.svg'];
          scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;
          return waitAfterNextRender(scannedImagesDiv);
        })
        .then(() => {
          // We expect the CSS variables to be set again.
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));
        });
  });

  // Tests that the remove page dialog opens, shows the correct page number,
  // then fires the correct event when the action button is clicked.
  test('removePageDialog', () => {
    const pageIndexToRemove = 5;
    let pageIndexFromEvent;
    scanPreview.addEventListener('remove-page', (e) => {
      pageIndexFromEvent = e.detail;
    });
    scanPreview.objectUrls = ['svg/ready_to_scan.svg'];
    return flushTasks()
        .then(() => {
          assertFalse(scanPreview.$$('#scanPreviewDialog').open);
          scanPreview.$$('action-toolbar')
              .dispatchEvent(new CustomEvent(
                  'show-remove-page-dialog', {detail: pageIndexToRemove}));
          return flushTasks();
        })
        .then(() => {
          assertTrue(scanPreview.$$('#scanPreviewDialog').open);
          assertEquals(
              'Remove page?',
              scanPreview.$$('#dialogTitle').textContent.trim());
          assertEquals(
              'Remove', scanPreview.$$('#actionButton').textContent.trim());
          assertEquals(
              loadTimeData.getStringF(
                  'removePageConfirmationText', pageIndexToRemove + 1),
              scanPreview.$$('#dialogConfirmationText').textContent.trim());

          scanPreview.$$('#actionButton').click();
          assertFalse(scanPreview.$$('#scanPreviewDialog').open);
          assertEquals(pageIndexToRemove, pageIndexFromEvent);
        });
  });

  // Tests that clicking the cancel button closes the remove page dialog.
  test('cancelRemovePageDialog', () => {
    scanPreview.objectUrls = ['image'];
    assertFalse(scanPreview.$$('#scanPreviewDialog').open);
    return flushTasks()
        .then(() => {
          scanPreview.$$('action-toolbar')
              .dispatchEvent(new CustomEvent('show-remove-page-dialog'));
          return flushTasks();
        })
        .then(() => {
          assertTrue(scanPreview.$$('#scanPreviewDialog').open);

          scanPreview.$$('#cancelButton').click();
          assertFalse(scanPreview.$$('#scanPreviewDialog').open);
        });
  });

  // Tests that the rescan page dialog opens and shows the correct page number.
  test('rescanPageDialog', () => {
    const pageIndex = 6;
    scanPreview.objectUrls = ['image1', 'image2'];
    assertFalse(scanPreview.$$('#scanPreviewDialog').open);
    return flushTasks()
        .then(() => {
          scanPreview.$$('action-toolbar')
              .dispatchEvent(new CustomEvent(
                  'show-rescan-page-dialog', {detail: pageIndex}));
          return flushTasks();
        })
        .then(() => {
          assertTrue(scanPreview.$$('#scanPreviewDialog').open);
          assertEquals(
              'Rescan page ' + (pageIndex + 1) + '?',
              scanPreview.$$('#dialogTitle').textContent.trim());
          assertEquals(
              loadTimeData.getStringF('rescanPageConfirmationText', pageIndex),
              scanPreview.$$('#dialogConfirmationText').textContent.trim());
        });
  });

  // Tests that for multi-page scans, resizing the app window triggers the
  // repositioning of the action toolbar.
  test('resizingWindowRepositionsActionToolbar', () => {
    const scannedImagesDiv =
        /** @type {!HTMLElement} */ (scanPreview.$$('#scannedImages'));
    scanPreview.objectUrls = ['image'];
    scanPreview.isMultiPageScan = true;
    scanPreview.appState = AppState.MULTI_PAGE_SCANNING;
    return flushTasks()
        .then(() => {
          scanPreview.objectUrls = ['svg/ready_to_scan.svg'];
          scanPreview.appState = AppState.MULTI_PAGE_NEXT_ACTION;
          return waitAfterNextRender(scannedImagesDiv);
        })
        .then(() => {
          // After the image loads we expect the CSS variables to be set.
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

          // Reset the CSS variables and simulate the window being resized.
          scanPreview.style.setProperty('--action-toolbar-top', '');
          scanPreview.style.setProperty('--action-toolbar-left', '');
          window.dispatchEvent(new CustomEvent('resize'));
          return flushTasks();
        })
        .then(() => {
          // The CSS variables should get set again.
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertNotEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));

          // Now test that unchecking the multi-page scan checkbox removes the
          // window listener.
          scanPreview.objectUrls = [];
          scanPreview.isMultiPageScan = false;
          scanPreview.appState = AppState.SCANNING;
          return flushTasks();
        })
        .then(() => {
          scanPreview.objectUrls = ['svg/ready_to_scan.svg'];
          scanPreview.appState = AppState.DONE;

          // Reset the CSS variables and simulate starting the window being
          // resized.
          scanPreview.style.setProperty('--action-toolbar-top', '');
          scanPreview.style.setProperty('--action-toolbar-left', '');
          window.dispatchEvent(new CustomEvent('resize'));
          return flushTasks();
        })
        .then(() => {
          // The CSS variables should not get set for non-multi-page scans.
          assertEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-top'));
          assertEquals(
              '', scanPreview.style.getPropertyValue('--action-toolbar-left'));
        });
  });
}
