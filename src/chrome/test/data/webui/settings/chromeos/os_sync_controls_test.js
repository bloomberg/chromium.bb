// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {OsSyncBrowserProxyImpl, Router, StatusAction, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.js';
// clang-format on

/** @implements {settings.OsSyncBrowserProxy} */
class TestOsSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'didNavigateToOsSyncPage',
      'didNavigateAwayFromOsSyncPage',
      'setOsSyncDatatypes',
    ]);
  }

  /** @override */
  didNavigateToOsSyncPage() {
    this.methodCalled('didNavigateToOsSyncPage');
  }

  /** @override */
  didNavigateAwayFromOsSyncPage() {
    this.methodCalled('didNavigateAwayFromSyncPage');
  }

  /** @override */
  setOsSyncDatatypes(osSyncPrefs) {
    this.methodCalled('setOsSyncDatatypes', osSyncPrefs);
  }
}

/**
 * Returns a sync prefs dictionary with either all or nothing syncing.
 * @param {boolean} syncAll
 * @return {!settings.OsSyncPrefs}
 */
function getOsSyncPrefs(syncAll) {
  return {
    osAppsRegistered: true,
    osAppsSynced: syncAll,
    osPreferencesRegistered: true,
    osPreferencesSynced: syncAll,
    syncAllOsTypes: syncAll,
    wallpaperEnabled: syncAll,
    wifiConfigurationsRegistered: true,
    wifiConfigurationsSynced: syncAll,
  };
}

function getSyncAllPrefs() {
  return getOsSyncPrefs(true);
}

function getSyncNothingPrefs() {
  return getOsSyncPrefs(false);
}

// Returns a SyncStatus representing the default syncing state.
function getDefaultSyncStatus() {
  return {
    disabled: false,
    hasError: false,
    hasUnrecoverableError: false,
    signedIn: true,
    statusAction: settings.StatusAction.NO_ACTION,
  };
}

function setupSync() {
  cr.webUIListenerCallback('os-sync-prefs-changed', getSyncAllPrefs());
  Polymer.dom.flush();
}

suite('OsSyncControlsTest', function() {
  let browserProxy = null;
  let syncControls = null;
  let syncIconContainer = null;

  setup(function() {
    browserProxy = new TestOsSyncBrowserProxy();
    settings.OsSyncBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    syncControls = document.createElement('os-sync-controls');
    syncControls.syncStatus = getDefaultSyncStatus();
    syncControls.profileName = 'John Cena';
    syncControls.profileEmail = 'john.cena@gmail.com';
    syncControls.profileIconUrl = 'data:image/png;base64,abc123';
    document.body.appendChild(syncControls);

    // Alias to help with line wrapping in test cases.
    syncIconContainer = syncControls.$.syncIconContainer;
  });

  teardown(function() {
    syncControls.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('ControlsHiddenUntilInitialUpdateSent', function() {
    assertTrue(syncControls.hidden);
    setupSync();
    assertFalse(syncControls.hidden);
  });

  test('Avatar icon', function() {
    assertEquals('data:image/png;base64,abc123', syncControls.$.avatarIcon.src);
  });

  test('Status icon is visible with feature enabled', function() {
    setupSync();
    assertFalse(syncControls.$.syncIconContainer.hidden);
  });

  test('Status icon with error', function() {
    setupSync();
    const status = getDefaultSyncStatus();
    status.hasError = true;
    syncControls.syncStatus = status;

    assertTrue(syncIconContainer.classList.contains('sync-problem'));
    assertTrue(!!syncControls.$$('[icon="settings:sync-problem"]'));
  });

  test('Status icon with sync paused for reauthentication', function() {
    setupSync();
    const status = getDefaultSyncStatus();
    status.hasError = true;
    status.statusAction = settings.StatusAction.REAUTHENTICATE;
    syncControls.syncStatus = status;

    assertTrue(syncIconContainer.classList.contains('sync-paused'));
    assertTrue(!!syncControls.$$('[icon="settings:sync-disabled"]'));
  });

  test('Status icon with sync disabled', function() {
    setupSync();
    const status = getDefaultSyncStatus();
    status.disabled = true;
    syncControls.syncStatus = status;

    assertTrue(syncIconContainer.classList.contains('sync-disabled'));
    assertTrue(!!syncControls.$$('[icon="cr:sync"]'));
  });

  test('Account name and email with feature enabled', function() {
    setupSync();
    assertEquals('John Cena', syncControls.$.accountTitle.textContent.trim());
    assertEquals(
        'Syncing to john.cena@gmail.com',
        syncControls.$.accountSubtitle.textContent.trim());
  });

  test('Account name and email with sync error', function() {
    setupSync();
    syncControls.syncStatus = {hasError: true};
    Polymer.dom.flush();
    assertEquals(
        `Sync isn't working`, syncControls.$.accountTitle.textContent.trim());
    assertEquals(
        'john.cena@gmail.com',
        syncControls.$.accountSubtitle.textContent.trim());
  });

  // Regression test for https://crbug.com/1076239
  test('Handles undefined syncStatus', function() {
    syncControls.syncStatus = undefined;
    setupSync();
    assertEquals('', syncControls.$.accountTitle.textContent.trim());
    assertEquals('', syncControls.$.accountSubtitle.textContent.trim());
  });

  test('SyncEnabled', function() {
    setupSync();

    assertFalse(syncControls.$.syncEverythingCheckboxLabel.hasAttribute(
        'label-disabled'));

    const syncAllControl = syncControls.$.syncAllOsTypesControl;
    assertFalse(syncAllControl.disabled);
    assertTrue(syncAllControl.checked);

    const labels = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > div.checkbox-label');
    for (const label of labels) {
      assertFalse(label.hasAttribute('label-disabled'));
    }

    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('UncheckingSyncAllEnablesAllIndividualControls', async function() {
    setupSync();
    syncControls.$.syncAllOsTypesControl.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('PrefChangeUpdatesControls', function() {
    const prefs = getSyncAllPrefs();
    prefs.syncAllOsTypes = false;
    cr.webUIListenerCallback('os-sync-prefs-changed', prefs);

    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertFalse(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('DisablingOneControlUpdatesPrefs', async function() {
    setupSync();

    // Disable "Sync All".
    syncControls.$.syncAllOsTypesControl.click();
    // Disable "Settings".
    syncControls.$.osPreferencesControl.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    expectedPrefs.osPreferencesSynced = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });
});

suite('OsSyncControlsNavigationTest', function() {
  test('DidNavigateEvents', async function() {
    const browserProxy = new TestOsSyncBrowserProxy();
    settings.OsSyncBrowserProxyImpl.instance_ = browserProxy;

    settings.Router.getInstance().navigateTo(settings.routes.OS_SYNC);
    await browserProxy.methodCalled('didNavigateToOsSyncPage');

    settings.Router.getInstance().navigateTo(settings.routes.OS_PEOPLE);
    await browserProxy.methodCalled('didNavigateAwayFromOsSyncPage');
  });
});
