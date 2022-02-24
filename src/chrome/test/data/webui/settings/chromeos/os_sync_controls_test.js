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
  let syncEverything = null;
  let customizeSync = null;

  setup(function() {
    browserProxy = new TestOsSyncBrowserProxy();
    settings.OsSyncBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    syncControls = document.createElement('os-sync-controls');
    document.body.appendChild(syncControls);

    syncEverything = syncControls.shadowRoot.querySelector(
        'cr-radio-button[name="sync-everything"]');
    customizeSync = syncControls.shadowRoot.querySelector(
        'cr-radio-button[name="customize-sync"]');
    assertTrue(!!syncEverything);
    assertTrue(!!customizeSync);
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

  test('SyncEnabled', function() {
    setupSync();

    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);

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
    customizeSync.click();
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

    // Select "Customize sync" instead of "Sync everything".
    customizeSync.click();
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
