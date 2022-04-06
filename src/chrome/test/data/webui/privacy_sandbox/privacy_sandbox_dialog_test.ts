// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_app.js';

import {PrivacySandboxDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_app.js';
import {PrivacySandboxDialogAction, PrivacySandboxDialogBrowserProxy} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_browser_proxy.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

class TestPrivacySandboxDialogBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxDialogBrowserProxy {
  constructor() {
    super(['dialogActionOccurred', 'resizeDialog', 'showDialog']);
  }

  dialogActionOccurred() {
    this.methodCalled('dialogActionOccurred', arguments);
  }

  resizeDialog() {
    this.methodCalled('resizeDialog', arguments);
    return Promise.resolve();
  }

  showDialog() {
    this.methodCalled('showDialog');
  }
}

suite('PrivacySandboxDialogConsent', function() {
  let page: PrivacySandboxDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(buttonSelector: string) {
    const actionButton =
        page.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isConsent: true,
    });
  });

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = '';
    page = document.createElement('privacy-sandbox-dialog-app');
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('dialogStructure', function() {
    // Consent dialog has addditionally an expand button and H2 title. It also
    // has a different set of buttons.
    assertTrue(isChildVisible(page, '.header h2'));
    assertTrue(isChildVisible(page, '.header h3'));

    assertTrue(isChildVisible(page, '.section'));

    assertTrue(isChildVisible(page, '#expandSection cr-expand-button'));

    assertTrue(isChildVisible(page, '#declineButton'));
    assertTrue(isChildVisible(page, '#confirmButton'));
    assertFalse(isChildVisible(page, '#settingsButton'));
    assertFalse(isChildVisible(page, '#ackButton'));
  });

  test('acceptClicked', async function() {
    testClickButton('#confirmButton');
    const [action] = await browserProxy.whenCalled('dialogActionOccurred');
    assertEquals(action, PrivacySandboxDialogAction.CONSENT_ACCEPTED);
  });

  test('declineClicked', async function() {
    testClickButton('#declineButton');
    const [action] = await browserProxy.whenCalled('dialogActionOccurred');
    assertEquals(action, PrivacySandboxDialogAction.CONSENT_DECLINED);
  });

  test('learnMoreClicked', async function() {
    // In the initial state, the content area isn't scrollable and doesn't have
    // a separator in the bottom (represented by 'can-scroll' class).
    // The collapse section is closed.
    const collapseElement = page.shadowRoot!.querySelector('iron-collapse');
    const contentArea: HTMLElement|null =
        page.shadowRoot!.querySelector('#contentArea');
    let hasScrollbar = contentArea!.offsetHeight < contentArea!.scrollHeight;
    assertFalse(collapseElement!.opened);
    assertEquals(contentArea!.classList.contains('can-scroll'), hasScrollbar);

    // After clicking on the collapse section, the content area expands and
    // becomes scrollable with a separator in the bottom. The collapse section
    // is opened and the native UI is notified about the action.
    testClickButton('#expandSection cr-expand-button');
    // TODO(crbug.com/1286276): Add testing for the scroll position.
    const [openedAction] =
        await browserProxy.whenCalled('dialogActionOccurred');
    assertEquals(
        openedAction, PrivacySandboxDialogAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);
    assertTrue(contentArea!.classList.contains('can-scroll'));

    // Reset proxy in between button clicks.
    browserProxy.reset();

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('#expandSection cr-expand-button');
    const [closedAction] =
        await browserProxy.whenCalled('dialogActionOccurred');
    hasScrollbar = contentArea!.offsetHeight < contentArea!.scrollHeight;
    assertEquals(
        closedAction, PrivacySandboxDialogAction.CONSENT_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
    assertEquals(contentArea!.classList.contains('can-scroll'), hasScrollbar);
  });
});

suite('PrivacySandboxDialogNotice', function() {
  let page: PrivacySandboxDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(buttonSelector: string) {
    const actionButton =
        page.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isConsent: false,
    });
  });

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = '';
    page = document.createElement('privacy-sandbox-dialog-app');
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('dialogStructure', function() {
    // Notice dialog doesn't have an expand button and H2 title. It also has
    // a different set of buttons.
    assertFalse(isChildVisible(page, '.header h2'));
    assertTrue(isChildVisible(page, '.header h3'));

    assertTrue(isChildVisible(page, '.section'));

    assertFalse(isChildVisible(page, '#expandSection cr-expand-button'));

    assertFalse(isChildVisible(page, '#declineButton'));
    assertFalse(isChildVisible(page, '#confirmButton'));
    assertTrue(isChildVisible(page, '#settingsButton'));
    assertTrue(isChildVisible(page, '#ackButton'));
  });

  test('ackClicked', async function() {
    testClickButton('#ackButton');
    const [action] = await browserProxy.whenCalled('dialogActionOccurred');
    assertEquals(action, PrivacySandboxDialogAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    testClickButton('#settingsButton');
    const [action] = await browserProxy.whenCalled('dialogActionOccurred');
    assertEquals(action, PrivacySandboxDialogAction.NOTICE_OPEN_SETTINGS);
  });
});
