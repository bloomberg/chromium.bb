// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createBoolPermission, getBoolPermissionValue, isBoolValue, setAppNotificationProviderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

class FakeAppNotificationHandler {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private
     *     {?chromeos.settings.appNotification.mojom.
     *      AppNotificationObserverRemote}
     */
    this.appNotificationObserverRemote_;

    this.isQuietModeEnabled_ = false;

    this.lastUpdatedAppId_ = -1;

    this.lastUpdatedAppPermission_ = {};

    /**
     * @private {!Array<!chromeos.settings.appNotification.mojom.App>}
     */
    this.apps_ = [];

    this.resetForTest();
  }

  resetForTest() {
    if (this.appNotificationObserverRemote_) {
      this.appNotificationObserverRemote_ = null;
    }

    this.apps_ = [];
    this.isQuietModeEnabled_ = false;
    this.lastUpdatedAppId_ = -1;
    this.lastUpdatedAppPermission_ = {};

    this.resolverMap_.set('addObserver', new PromiseResolver());
    this.resolverMap_.set('setQuietMode', new PromiseResolver());
    this.resolverMap_.set('setNotificationPermission', new PromiseResolver());
    this.resolverMap_.set('notifyPageReady', new PromiseResolver());
    this.resolverMap_.set('getApps', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @return
   *      {chromeos.settings.appNotification.mojom.
   *        AppNotificationObserverRemote}
   */
  getObserverRemote() {
    return this.appNotificationObserverRemote_;
  }

  /**
   * @return {boolean} quietModeState
   */
  getCurrentQuietModeState() {
    return this.isQuietModeEnabled_;
  }

  /**
   * @return {number}
   */
  getLastUpdatedAppId() {
    return this.lastUpdatedAppId_;
  }

  /**
   * @return {!apps.mojom.Permission}
   */
  getLastUpdatedPermission() {
    return this.lastUpdatedAppPermission_;
  }

  // appNotificationHandler methods

  /**
   * @param {!chromeos.settings.appNotification.mojom.
   *        AppNotificationObserverRemote}
   *      remote
   * @return {!Promise}
   */
  addObserver(remote) {
    return new Promise(resolve => {
      this.appNotificationObserverRemote_ = remote;
      this.methodCalled('addObserver');
      resolve();
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  setQuietMode(enabled) {
    this.isQuietModeEnabled_ = enabled;
    return new Promise(resolve => {
      this.methodCalled('setQuietMode');
      resolve({success: true});
    });
  }

  /**
   * @param {string} id
   * @param {!apps.mojom.Permission} permission
   */
  setNotificationPermission(id, permission) {
    return new Promise(resolve => {
      this.lastUpdatedAppId_ = id;
      this.lastUpdatedAppPermission_ = permission;
      this.methodCalled('setNotificationPermission');
      resolve({success: true});
    });
  }

  /** @return {!Promise} */
  notifyPageReady() {
    return new Promise(resolve => {
      this.methodCalled('notifyPageReady');
      resolve();
    });
  }

  /**
   * @return {!Promise<!Array<!chromeos.settings.appNotification.mojom.App>>}
   */
  getApps() {
    return new Promise(resolve => {
      this.methodCalled('getApps');
      resolve({apps: this.apps_});
    });
  }
}


suite('AppNotificationsSubpageTests', function() {
  /** @type {AppNotificationsSubpage} */
  let page;

  /**
   * @type {
   *    ?chromeos.settings.appNotification.mojom.AppNotificationHandlerRemote
   *  }
   */
  let mojoApi_;

  let setQuietModeCounter = 0;

  suiteSetup(() => {
    mojoApi_ = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi_);
  });

  setup(function() {
    PolymerTest.clearBody;
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
    page = document.createElement('settings-app-notifications-subpage');
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    page.remove();
    page = null;
  });

  /**
   * @return {!Promise}
   */
  function initializeObserver() {
    return mojoApi_.whenCalled('addObserver');
  }

  function simulateQuickSettings(enable) {
    mojoApi_.getObserverRemote().onQuietModeChanged(enable);
  }

  /** @param {!chromeos.settings.appNotification.mojom.App} */
  function simulateNotificationAppChanged(app) {
    mojoApi_.getObserverRemote().onNotificationAppChanged(app);
  }

  /**
   * @param {string} id
   * @param {string} title
   * @param {!apps.mojom.Permission} permission
   * @param {?apps.mojom.Readiness} readiness
   * @return {!chromeos.settings.appNotification.mojom.App}
   */
  function createApp(
      id, title, permission, readiness = apps.mojom.Readiness.kReady) {
    return {
      id: id,
      title: title,
      notificationPermission: permission,
      readiness: readiness
    };
  }

  test('loadAppListAndClickToggle', async () => {
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ false);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('1', 'App1', permission1);
    const app2 = createApp('2', 'App2', permission2);

    await initializeObserver;
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    // Expect 2 apps to be loaded.
    const appRowList =
        page.$.appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(2, appRowList.length);

    const appRow1 = appRowList[0];
    assertFalse(appRow1.$.appToggle.checked);
    assertFalse(appRow1.$.appToggle.disabled);
    assertEquals('App1', appRow1.$.appTitle.textContent.trim());

    const appRow2 = appRowList[1];
    assertTrue(appRow2.$.appToggle.checked);
    assertFalse(appRow2.$.appToggle.disabled);
    assertEquals('App2', appRow2.$.appTitle.textContent.trim());

    // Click on the first app's toggle.
    appRow1.$.appToggle.click();

    await mojoApi_.whenCalled('setNotificationPermission');

    // Verify that the sent message matches the app it was clicked from.
    assertEquals('1', mojoApi_.getLastUpdatedAppId());
    const lastUpdatedPermission = mojoApi_.getLastUpdatedPermission();
    assertEquals(1, lastUpdatedPermission.permissionType);
    assertTrue(isBoolValue(lastUpdatedPermission.value));
    assertEquals(false, lastUpdatedPermission.isManaged);
    assertTrue(getBoolPermissionValue(lastUpdatedPermission.value));
  });

  test('RemovedApp', async () => {
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ false);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('1', 'App1', permission1);
    const app2 = createApp('2', 'App2', permission2);

    await initializeObserver;
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    // Expect 2 apps to be loaded.
    let appRowList =
        page.$.appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(2, appRowList.length);

    const app3 = createApp(
        '1', 'App1', permission1, apps.mojom.Readiness.kUninstalledByUser);
    simulateNotificationAppChanged(app3);

    await flushTasks();
    // Expect only 1 app to be shown now.
    appRowList =
        page.$.appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(1, appRowList.length);

    const appRow1 = appRowList[0];
    assertEquals('App2', appRow1.$.appTitle.textContent.trim());
  });

  test('Each app-notification-row displays correctly', async () => {
    const appTitle1 = 'Files';
    const appTitle2 = 'Chrome';
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ true);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('file-id', appTitle1, permission1);
    const app2 = createApp('chrome-id', appTitle2, permission2);

    await initializeObserver;
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    const chromeRow = page.$.appNotificationsList.children[0];
    const filesRow = page.$.appNotificationsList.children[1];

    assertTrue(!!page);
    flush();

    // Apps should be listed in alphabetical order. |appTitle1| should come
    // before |appTitle2|, so a 1 should be returned by localCompare.
    const expected = 1;
    const actual = appTitle1.localeCompare(appTitle2);
    assertEquals(expected, actual);

    assertEquals(appTitle2, chromeRow.$.appTitle.textContent.trim());
    assertFalse(chromeRow.$.appToggle.disabled);
    assertFalse(!!chromeRow.shadowRoot.querySelector('cr-policy-indicator'));

    assertEquals(appTitle1, filesRow.$.appTitle.textContent.trim());
    assertTrue(filesRow.$.appToggle.disabled);
    assertTrue(!!filesRow.shadowRoot.querySelector('cr-policy-indicator'));
  });

  test('toggleDoNotDisturb', function() {
    const div = page.$.enableDoNotDisturb;
    const dndToggle = page.$.enableDndToggle;

    flush();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi_.getCurrentQuietModeState());

    // Test that tapping the single settings-box div enables DND.
    assertTrue(!!div);
    div.click();
    assertTrue(dndToggle.checked);
    assertTrue(mojoApi_.getCurrentQuietModeState());

    // Click again will change the value.
    div.click();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi_.getCurrentQuietModeState());

    // Test that tapping the toggle itself enables DND.
    dndToggle.click();
    assertTrue(dndToggle.checked);
    assertTrue(mojoApi_.getCurrentQuietModeState());
  });

  test('SetQuietMode being called correctly', function() {
    const dndToggle = page.$.enableDndToggle;
    // Click the toggle button a certain count.
    const testCount = 3;

    flush();
    assertFalse(dndToggle.checked);

    // Verify that every toggle click makes a call to setQuietMode and is
    // counted accordingly.
    for (let i = 0; i < testCount; i++) {
      return initializeObserver()
          .then(() => {
            flush();
            dndToggle.click();
            return mojoApi_.whenCalled('setQuietMode').then(() => {
              setQuietModeCounter++;
            });
          })
          .then(() => {
            flush();
            assertEquals(i + 1, setQuietModeCounter);
          });
    }
  });

  test('changing toggle with OnQuietModeChanged', function() {
    const dndToggle = page.$.enableDndToggle;

    flush();
    assertFalse(dndToggle.checked);

    // Verify that calling onQuietModeChanged sets toggle value.
    // This is equivalent to changing the DoNotDisturb setting in quick
    // settings.
    return initializeObserver()
        .then(() => {
          flush();
          simulateQuickSettings(/** enable */ true);
          return flushTasks();
        })
        .then(() => {
          assertTrue(dndToggle.checked);
        });
  });
});
