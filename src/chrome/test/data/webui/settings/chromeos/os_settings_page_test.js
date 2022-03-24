// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {CrSettingsPrefs, Router, routes, setNearbyShareSettingsForTesting, setContactManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeBluetoothConfig} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
// #import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';
// #import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
// clang-format on

suite('OsSettingsPageTests', function() {
  /** @type {?OsSettingsPageElement} */
  let settingsPage = null;

  /** @type {?SettingsPrefsElement} */
  let prefElement = null;

  /** @type {!nearby_share.FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!nearby_share.FakeNearbyShareSettings} */
  let fakeSettings = null;

  suiteSetup(async function() {
    loadTimeData.overrideValues({
      enableBluetoothRevamp: false,
    });

    fakeContactManager = new nearby_share.FakeContactManager();
    nearby_share.setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new nearby_share.FakeNearbyShareSettings();
    nearby_share.setNearbyShareSettingsForTesting(fakeSettings);

    settings.Router.getInstance().navigateTo(settings.routes.BASIC);
    PolymerTest.clearBody();

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    settingsPage.remove();
    CrSettingsPrefs.resetForTesting();
    settings.Router.getInstance().resetRouteForTesting();
  });

  function init() {
    settingsPage = document.createElement('os-settings-page');
    settingsPage.prefs = prefElement.prefs;
    document.body.appendChild(settingsPage);
    Polymer.dom.flush();
  }

  test('Os Settings Page created', async () => {
    init();
    assert(!!settingsPage);
  });

  test('Check settings-internet-page exists', async () => {
    init();
    const settingsInternetPage = settingsPage.$$('settings-internet-page');
    assert(!!settingsInternetPage);
  });

  test('Check os-settings-printing-page exists', async () => {
    init();
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const osSettingsPrintingPage = settingsPage.$$('os-settings-printing-page');
    assert(!!osSettingsPrintingPage);
  });

  test(
      'Check settings-bluetooth-page exists with' +
          'enableBluetoothRevamp flag off',
      async () => {
        init();
        const settingsBluetoothPage =
            settingsPage.$$('settings-bluetooth-page');

        const osSettingsBluetoothPage =
            settingsPage.$$('os-settings-bluetooth-page');
        assert(!!settingsBluetoothPage);
        assertFalse(!!osSettingsBluetoothPage);
      });

  test(
      'Check settings-bluetooth-page does not exist with' +
          'enableBluetoothRevamp flag on',
      async () => {
        loadTimeData.overrideValues({
          enableBluetoothRevamp: true,
        });

        // Using the real CrosBluetoothConfig will crash due to no
        // SessionManager.
        setBluetoothConfigForTesting(new FakeBluetoothConfig());

        init();
        const settingsBluetoothPage =
            settingsPage.$$('settings-bluetooth-page');

        const osSettingsBluetoothPage =
            settingsPage.$$('os-settings-bluetooth-page');

        assertFalse(!!settingsBluetoothPage);
        assert(!!osSettingsBluetoothPage);
      });

  test('Check os-settings-privacy-page exists', async () => {
    init();
    const osSettingsPrivacyPage = settingsPage.$$('os-settings-privacy-page');
    assert(!!osSettingsPrivacyPage);
    Polymer.dom.flush();
  });

  test('Check settings-multidevice-page exists', async () => {
    init();
    const settingsMultidevicePage =
        settingsPage.$$('settings-multidevice-page');
    assert(!!settingsMultidevicePage);
  });

  test('Check os-settings-people-page exists', async () => {
    init();
    const settingsPeoplePage = settingsPage.$$('os-settings-people-page');
    assert(!!settingsPeoplePage);
  });

  test('Check settings-date-time-page exists', async () => {
    init();
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const settingsDateTimePage = settingsPage.$$('settings-date-time-page');
    assert(!!settingsDateTimePage);
  });

  test('Check os-settings-languages-section exists', async () => {
    init();
    const idleRender = settingsPage.$$('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    Polymer.dom.flush();
    const osSettingsLangagesSection =
        settingsPage.$$('os-settings-languages-section');
    assert(!!osSettingsLangagesSection);
  });

  test('Check os-settings-a11y-page exists', async () => {
    init();
    const idleRender = settingsPage.$$('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    Polymer.dom.flush();
    const osSettingsA11yPage = settingsPage.$$('os-settings-a11y-page');
    assert(!!osSettingsA11yPage);
  });

  test('Check settings-kerberos-page exists', async () => {
    init();
    settingsPage.showKerberosSection = true;
    const idleRender = settingsPage.$$('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    Polymer.dom.flush();

    const settingsKerberosPage = settingsPage.$$('settings-kerberos-page');
    assert(!!settingsKerberosPage);
    Polymer.dom.flush();
  });

  test('Check settings-device-page exists', async () => {
    init();
    settingsPage.showCrostini = true;
    settingsPage.allowCrostini_ = true;
    const settingsDevicePage = settingsPage.$$('settings-device-page');
    assert(!!settingsDevicePage);
    Polymer.dom.flush();
  });

  test('Check os-settings-files-page exists', async () => {
    init();
    settingsPage.isGuestMode_ = false;
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const settingsFilesPage = settingsPage.$$('os-settings-files-page');
    assert(!!settingsFilesPage);
  });

  test('Check settings-personalization-page exists', async () => {
    init();
    const settingsPersonalizationPage =
        settingsPage.$$('settings-personalization-page');
    assert(!!settingsPersonalizationPage);
  });

  test('Check os-settings-search-page exists', async () => {
    init();
    const osSettingsSearchPage = settingsPage.$$('os-settings-search-page');
    assert(!!osSettingsSearchPage);
  });

  test('Check os-settings-apps-page exists', async () => {
    init();
    settingsPage.showAndroidApps = true;
    settingsPage.showPluginVm = true;
    settingsPage.havePlayStoreApp = true;
    Polymer.dom.flush();
    const osSettingsAppsPage = settingsPage.$$('os-settings-apps-page');
    assert(!!osSettingsAppsPage);
  });

  test('Check settings-crostini-page exists', async () => {
    init();
    settingsPage.showCrostini = true;
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const osSettingsCrostiniPage = settingsPage.$$('settings-crostini-page');
    assert(!!osSettingsCrostiniPage);
  });

  test('Check os-settings-reset-page exists', async () => {
    init();
    const idleRender = settingsPage.$$('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();

    settingsPage.showReset = true;
    Polymer.dom.flush();
    const osSettingsResetPage = settingsPage.$$('os-settings-reset-page');
    assert(!!osSettingsResetPage);
  });
});
