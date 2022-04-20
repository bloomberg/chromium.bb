// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {IGNORE_APP_ERRORS, remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Tests that Guest OS entries don't show up if the flag controlling guest os +
 * files app integration is disabled.
 */
testcase.notListedWithoutFlag = async () => {
  // Prepopulate the list with a bunch of guests.
  const names = ['Electra', 'Etcetera', 'Jemima'];
  for (const name of names) {
    await sendTestMessage({name: 'registerMountableGuest', displayName: name});
  }

  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Check that we have no Guest OS entries.
  const query = '#directory-tree [root-type-icon=guest_os]';
  await remoteCall.waitForElementsCount(appId, [query], 0);
};

/**
 * Tests that Guest OS entries show up in the sidebar at files app launch.
 */
testcase.fakesListed = async () => {
  // Prepopulate the list with a bunch of guests.
  const names = ['Electra', 'Etcetera', 'Jemima'];
  for (const name of names) {
    await sendTestMessage({name: 'registerMountableGuest', displayName: name});
  }

  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Check that our guests are listed.
  for (const name of names) {
    await remoteCall.waitForElement(
        appId, `#directory-tree [entry-label="${name}"]`);
  }
};

/**
 * Tests that the list of guests is updated when new guests are added or
 * removed.
 */
testcase.listUpdatedWhenGuestsChanged = async () => {
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // We'll use this query a lot to check how many guests we have.
  const query = '#directory-tree [root-type-icon=guest_os]';

  const names = ['Etcetera', 'Electra'];
  const ids = [];

  for (const name of names) {
    // Add a guest...
    ids.push(await sendTestMessage(
        {name: 'registerMountableGuest', displayName: name}));

    // ...and it should show up
    await remoteCall.waitForElement(
        appId, `#directory-tree [entry-label="${name}"]`);
  }

  // Check that we have the right number of entries.
  await remoteCall.waitForElementsCount(appId, [query], ids.length);

  // Remove the guests...
  for (const guestId of ids) {
    await sendTestMessage({name: 'unregisterMountableGuest', guestId: guestId});
  }

  // ...and they should all be gone.
  await remoteCall.waitForElementsCount(appId, [query], 0);

  // Then add them back for good measure.
  for (const name of names) {
    await sendTestMessage({name: 'registerMountableGuest', displayName: name});
  }
  await remoteCall.waitForElementsCount(appId, [query], names.length);
};

/**
 * Tests that clicking on a Guest OS entry in the sidebar mounts the
 * corresponding volume, and that the UI is update appropriately (volume in
 * sidebar and not fake, contents show up once done loading, etc).
 */
testcase.mountGuestSuccess = async () => {
  const guestName = 'JennyAnyDots';
  // Start off with one guest.
  const id = await sendTestMessage(
      {name: 'registerMountableGuest', displayName: guestName, canMount: true});
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Wait for our guest to appear and click it.
  const query = '#directory-tree [root-type-icon=guest_os]';
  await remoteCall.waitAndClickElement(appId, query);

  // Wait until it's loaded.
  await remoteCall.waitForElement(
      appId, `#breadcrumbs[path="My files/${guestName}"]`);

  // We should have a volume in the sidebar
  await remoteCall.waitForElement(
      appId,
      `.tree-item[volume-type-for-testing="guest_os"][entry-label="${
          guestName}"]`);

  // We should no longer have a fake
  await remoteCall.waitForElementsCount(appId, [query], 0);

  // And the volume should be focused in the main window
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="${guestName}"]`);

  // It should not be read-only
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');
};
