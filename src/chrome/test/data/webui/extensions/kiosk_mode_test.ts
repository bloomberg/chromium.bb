// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-kiosk-dialog. */

import {CrCheckboxElement, ExtensionsKioskDialogElement, KioskApp, KioskAppSettings, KioskBrowserProxyImpl, KioskSettings} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestKioskBrowserProxy} from './test_kiosk_browser_proxy.js';

const extension_kiosk_mode_tests = {
  suiteName: 'kioskModeTests',
  TestNames: {
    AddButton: 'AddButton',
    AddError: 'AddError',
    AutoLaunch: 'AutoLaunch',
    Bailout: 'Bailout',
    Layout: 'Layout',
    Updated: 'Updated',
  },
};

Object.assign(window, {extension_kiosk_mode_tests});

suite(extension_kiosk_mode_tests.suiteName, function() {
  let browserProxy: TestKioskBrowserProxy;

  let dialog: ExtensionsKioskDialogElement;

  const basicApps: KioskApp[] = [
    {
      id: 'app_1',
      name: 'App1 Name',
      iconURL: '',
      autoLaunch: false,
      isLoading: false,
    },
    {
      id: 'app_2',
      name: 'App2 Name',
      iconURL: '',
      autoLaunch: false,
      isLoading: false,
    },
  ];

  function setAppSettings(settings: Partial<KioskAppSettings>) {
    const appSettings = {
      apps: [],
      disableBailout: false,
      hasAutoLaunchApp: false,
    };

    browserProxy.setAppSettings(Object.assign(appSettings, settings));
  }

  function setInitialSettings(settings: Partial<KioskSettings>) {
    const initialSettings = {
      kioskEnabled: true,
      autoLaunchEnabled: false,
    };

    browserProxy.setInitialSettings(Object.assign(initialSettings, settings));
  }

  function initPage(): Promise<void> {
    document.body.innerHTML = '';
    browserProxy.reset();
    dialog = document.createElement('extensions-kiosk-dialog');
    document.body.appendChild(dialog);

    return browserProxy.whenCalled('getKioskAppSettings')
        .then(() => flushTasks());
  }

  setup(function() {
    browserProxy = new TestKioskBrowserProxy();
    setAppSettings({apps: basicApps.slice(0)});
    KioskBrowserProxyImpl.setInstance(browserProxy);

    return initPage();
  });

  test(assert(extension_kiosk_mode_tests.TestNames.Layout), function() {
    const apps = basicApps.slice(0);
    apps[1]!.autoLaunch = true;
    apps[1]!.isLoading = true;
    setAppSettings({apps: apps, hasAutoLaunchApp: true});

    return initPage()
        .then(() => {
          const items =
              dialog.shadowRoot!.querySelectorAll<HTMLElement>('.list-item');
          assertEquals(items.length, 2);
          assertTrue(items[0]!.textContent!.includes(basicApps[0]!.name));
          assertTrue(items[1]!.textContent!.includes(basicApps[1]!.name));
          // Second item should show the auto-lauch label.
          assertTrue(items[0]!.querySelector('span')!.hidden);
          assertFalse(items[1]!.querySelector('span')!.hidden);
          // No permission to edit auto-launch so buttons should be hidden.
          assertTrue(items[0]!.querySelector('cr-button')!.hidden);
          assertTrue(items[1]!.querySelector('cr-button')!.hidden);
          // Bailout checkbox should be hidden when auto-launch editing
          // disabled.
          assertTrue(dialog.shadowRoot!.querySelector('cr-checkbox')!.hidden);

          items[0]!.querySelector<HTMLElement>('.icon-delete-gray')!.click();
          flush();
          return browserProxy.whenCalled('removeKioskApp');
        })
        .then(appId => {
          assertEquals(appId, basicApps[0]!.id);
        });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.AutoLaunch), function() {
    const apps = basicApps.slice(0);
    apps[1]!.autoLaunch = true;
    setAppSettings({apps: apps, hasAutoLaunchApp: true});
    setInitialSettings({autoLaunchEnabled: true});

    let buttons: NodeListOf<HTMLElement>;
    return initPage()
        .then(() => {
          buttons = dialog.shadowRoot!.querySelectorAll<HTMLElement>(
              '.list-item cr-button');
          // Has permission to edit auto-launch so buttons should be seen.
          assertFalse(buttons[0]!.hidden);
          assertFalse(buttons[1]!.hidden);

          buttons[0]!.click();
          return browserProxy.whenCalled('enableKioskAutoLaunch');
        })
        .then(appId => {
          assertEquals(appId, basicApps[0]!.id);

          buttons[1]!.click();
          return browserProxy.whenCalled('disableKioskAutoLaunch');
        })
        .then(appId => {
          assertEquals(appId, basicApps[1]!.id);
        });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.Bailout), function() {
    const apps = basicApps.slice(0);
    apps[1]!.autoLaunch = true;
    setAppSettings({apps: apps, hasAutoLaunchApp: true});
    setInitialSettings({autoLaunchEnabled: true});

    assertFalse(dialog.$.confirmDialog.open);

    let bailoutCheckbox: CrCheckboxElement;
    return initPage()
        .then(() => {
          bailoutCheckbox = dialog.shadowRoot!.querySelector('cr-checkbox')!;
          // Bailout checkbox should be usable when auto-launching.
          assertFalse(bailoutCheckbox.hidden);
          assertFalse(bailoutCheckbox.disabled);
          assertFalse(bailoutCheckbox.checked);

          // Making sure canceling doesn't change anything.
          bailoutCheckbox.click();
          flush();
          assertTrue(dialog.$.confirmDialog.open);

          dialog.$.confirmDialog.querySelector<HTMLElement>(
                                    '.cancel-button')!.click();
          flush();
          assertFalse(bailoutCheckbox.checked);
          assertFalse(dialog.$.confirmDialog.open);
          assertTrue(dialog.$.dialog.open);

          // Accepting confirmation dialog should trigger browserProxy call.
          bailoutCheckbox.click();
          flush();
          assertTrue(dialog.$.confirmDialog.open);

          dialog.$.confirmDialog.querySelector<HTMLElement>(
                                    '.action-button')!.click();
          flush();
          assertTrue(bailoutCheckbox.checked);
          assertFalse(dialog.$.confirmDialog.open);
          assertTrue(dialog.$.dialog.open);
          return browserProxy.whenCalled('setDisableBailoutShortcut');
        })
        .then(disabled => {
          assertTrue(disabled);

          // Test clicking on checkbox again should simply re-enable bailout.
          browserProxy.reset();
          bailoutCheckbox.click();
          assertFalse(bailoutCheckbox.checked);
          assertFalse(dialog.$.confirmDialog.open);
          return browserProxy.whenCalled('setDisableBailoutShortcut');
        })
        .then(disabled => {
          assertFalse(disabled);
        });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.AddButton), function() {
    const addButton = dialog.$.addButton;
    assertTrue(!!addButton);
    assertTrue(addButton.disabled);

    const addInput = dialog.$.addInput;
    addInput.value = 'blah';
    assertFalse(addButton.disabled);

    addButton.click();
    return browserProxy.whenCalled('addKioskApp').then(appId => {
      assertEquals(appId, 'blah');
    });
  });

  test(assert(extension_kiosk_mode_tests.TestNames.Updated), function() {
    const items =
        dialog.shadowRoot!.querySelectorAll<HTMLElement>('.list-item');
    assertTrue(items[0]!.textContent!.includes(basicApps[0]!.name));

    const newName = 'completely different name';

    webUIListenerCallback('kiosk-app-updated', {
      id: basicApps[0]!.id,
      name: newName,
      iconURL: '',
      autoLaunch: false,
      isLoading: false,
    });

    assertFalse(items[0]!.textContent!.includes(basicApps[0]!.name));
    assertTrue(items[0]!.textContent!.includes(newName));
  });

  test(assert(extension_kiosk_mode_tests.TestNames.AddError), function() {
    const addInput = dialog.$.addInput;

    assertFalse(!!addInput.invalid);
    webUIListenerCallback('kiosk-app-error', basicApps[0]!.id);

    assertTrue(!!addInput.invalid);
    assertTrue(addInput.errorMessage!.includes(basicApps[0]!.id));
  });
});
