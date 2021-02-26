// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, getSelectDropdownBackground} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Base} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {getGoogleDriveDestination, selectOption} from './print_preview_test_utils.js';

window.destination_select_test = {};
const destination_select_test = window.destination_select_test;
destination_select_test.suiteName = 'DestinationSelectTest';
/** @enum {string} */
destination_select_test.TestNames = {
  UpdateStatus: 'update status',
  ChangeIcon: 'change icon',
};

suite(destination_select_test.suiteName, function() {
  /** @type {!PrintPreviewDestinationSelectElement} */
  let destinationSelect;

  /** @type {string} */
  const account = 'foo@chromium.org';

  /** @type {!DestinationOrigin} */
  const cookieOrigin = DestinationOrigin.COOKIES;

  /** @type {string} */
  const driveKey =
      `${Destination.GooglePromotedId.DOCS}/${cookieOrigin}/${account}`;

  /** @type {!Array<!Destination>} */
  let recentDestinationList = [];

  const meta = /** @type {!IronMetaElement} */ (
      Base.create('iron-meta', {type: 'iconset'}));

  /** @override */
  setup(function() {
    document.body.innerHTML = '';
    destinationSelect =
        /** @type {!PrintPreviewDestinationSelectElement} */ (
            document.createElement('print-preview-destination-select'));
    destinationSelect.activeUser = account;
    destinationSelect.appKioskMode = false;
    destinationSelect.disabled = false;
    destinationSelect.loaded = false;
    destinationSelect.noDestinations = false;
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    document.body.appendChild(destinationSelect);
  });

  // Create three different destinations and use them to populate
  // |recentDestinationList|.
  function populateRecentDestinationList() {
    recentDestinationList = [
      new Destination(
          'ID1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
          DestinationConnectionStatus.ONLINE),
      new Destination(
          'ID2', DestinationType.GOOGLE, cookieOrigin, 'Two',
          DestinationConnectionStatus.OFFLINE, {account: account}),
      new Destination(
          'ID3', DestinationType.GOOGLE, cookieOrigin, 'Three',
          DestinationConnectionStatus.ONLINE,
          {account: account, isOwned: true}),
    ];
  }

  function compareIcon(selectEl, expectedIcon) {
    const icon = selectEl.style['background-image'].replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        /** @type {!IronIconsetSvgElement} */ (meta.byKey('print-preview')),
        expectedIcon, destinationSelect);
    assertEquals(expected, icon);
  }

  /**
   * Test that changing different destinations results in the correct icon being
   * shown.
   * @return {!Promise} Promise that resolves when the test finishes.
   */
  function testChangeIcon() {
    const destination = recentDestinationList[0];
    destinationSelect.destination = destination;
    destinationSelect.updateDestination();
    destinationSelect.loaded = true;
    const selectEl = destinationSelect.$$('.md-select');
    compareIcon(selectEl, 'print');
    destinationSelect.driveDestinationKey = driveKey;

    return selectOption(destinationSelect, driveKey)
        .then(() => {
          // Icon updates early based on the ID.
          compareIcon(selectEl, 'save-to-drive');

          // Update the destination.
          destinationSelect.destination = getGoogleDriveDestination(account);

          // Still Save to Drive icon.
          compareIcon(selectEl, 'save-to-drive');

          // Select a destination with the shared printer icon.
          return selectOption(
              destinationSelect, `ID2/${cookieOrigin}/${account}`);
        })
        .then(() => {
          // Should already be updated.
          compareIcon(selectEl, 'printer-shared');

          // Update destination.
          destinationSelect.destination = recentDestinationList[1];
          compareIcon(selectEl, 'printer-shared');

          // Select a destination with a standard printer icon.
          return selectOption(
              destinationSelect, `ID3/${cookieOrigin}/${account}`);
        })
        .then(() => {
          compareIcon(selectEl, 'print');

          // Update destination.
          destinationSelect.destination = recentDestinationList[2];
          compareIcon(selectEl, 'print');
        });
  }

  /**
   * Test that changing different destinations results in the correct status
   * being shown.
   */
  function testUpdateStatus() {
    loadTimeData.overrideValues({
      offline: 'offline',
    });

    assertFalse(destinationSelect.$$('.throbber-container').hidden);
    assertTrue(destinationSelect.$$('.md-select').hidden);

    destinationSelect.loaded = true;
    assertTrue(destinationSelect.$$('.throbber-container').hidden);
    assertFalse(destinationSelect.$$('.md-select').hidden);

    const additionalInfoEl =
        destinationSelect.$$('.destination-additional-info');
    const statusEl = destinationSelect.$$('.destination-status');

    destinationSelect.driveDestinationKey = driveKey;
    destinationSelect.destination = getGoogleDriveDestination(account);
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[0];
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[1];
    destinationSelect.updateDestination();
    assertFalse(additionalInfoEl.hidden);
    assertEquals('offline', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[2];
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);
  }

  test(assert(destination_select_test.TestNames.UpdateStatus), function() {
    loadTimeData.overrideValues(
        {cloudPrintDeprecationWarningsSuppressed: true});

    // Repopulate |recentDestinationList| to have
    // |cloudPrintDeprecationWarningsSuppressed| take effect during creation of
    // new Destinations.
    populateRecentDestinationList();
    return testUpdateStatus();
  });

  test(assert(destination_select_test.TestNames.ChangeIcon), function() {
    loadTimeData.overrideValues(
        {cloudPrintDeprecationWarningsSuppressed: true});

    // Repopulate |recentDestinationList| to have
    // |cloudPrintDeprecationWarningsSuppressed| take effect during creation of
    // new Destinations.
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    return testChangeIcon();
  });
});
