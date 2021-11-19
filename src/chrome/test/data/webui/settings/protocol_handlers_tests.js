// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
// clang-format on

/** @fileoverview Suite of tests for protocol_handlers. */
suite('ProtocolHandlers', function() {
  /**
   * A dummy protocol handler element created before each test.
   * @type {ProtocolHandlers}
   */
  let testElement;

  /**
   * A list of ProtocolEntry fixtures.
   * @type {!Array<!ProtocolEntry>}
   */
  const protocols = [
    {
      handlers: [{
        host: 'www.google.com',
        protocol: 'mailto',
        protocol_name: 'email',
        spec: 'http://www.google.com/%s',
        is_default: true
      }],
      protocol: 'mailto'
    },
    {
      handlers: [
        {
          host: 'www.google1.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google1.com/%s',
          is_default: true
        },
        {
          host: 'www.google2.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google2.com/%s',
          is_default: false
        }
      ],
      protocol: 'webcal'
    }
  ];

  /**
   * A list of AppProtocolEntry fixtures.
   * @type {!Array<!AppProtocolEntry>}
   */
  const appAllowedProtocols = [
    {
      handlers: [{
        host: 'www.google.com',
        protocol: 'mailto',
        protocol_name: 'email',
        spec: 'http://www.google.com/%s',
        app_id: 'testID'
      }],
      protocol: 'mailto'
    },
    {
      handlers: [
        {
          host: 'www.google1.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google1.com/%s',
          app_id: 'testID1'
        },
        {
          host: 'www.google2.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google2.com/%s',
          app_id: 'testID2'
        }
      ],
      protocol: 'webcal'
    }
  ];

  /**
   * A list of AppProtocolEntry fixtures. This list should only contain
   * entries that do not overlap `appAllowedProtocols`.
   * @type {!Array<!AppProtocolEntry>}
   */
  const appDisallowedProtocols = [
    {
      handlers: [{
        host: 'www.google1.com',
        protocol: 'mailto',
        protocol_name: 'email',
        spec: 'http://www.google1.com/%s',
        app_id: 'testID1'
      }],
      protocol: 'mailto'
    },
    {
      handlers: [
        {
          host: 'www.google.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google.com/%s',
          app_id: 'testID'
        },
        {
          host: 'www.google3.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google3.com/%s',
          app_id: 'testID3'
        }
      ],
      protocol: 'webcal'
    }
  ];

  /**
   * A list of IgnoredProtocolEntry fixtures.
   * @type {!Array<!HandlerEntry}>}
   */
  const ignoredProtocols = [{
    host: 'www.google.com',
    protocol: 'web+ignored',
    protocol_name: 'web+ignored',
    spec: 'https://www.google.com/search?q=ignored+%s',
    is_default: false
  }];

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  setup(async function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
  });

  teardown(function() {
    testElement.remove();
    testElement = null;
  });

  /** @return {!Promise} */
  function initPage() {
    browserProxy.reset();
    PolymerTest.clearBody();
    testElement = document.createElement('protocol-handlers');
    document.body.appendChild(testElement);
    return browserProxy.whenCalled('observeAppProtocolHandlers')
        .then(function() {
          flush();
        });
  }

  test('set protocol handlers default called', () => {
    return initPage().then(() => {
      testElement.shadowRoot.querySelector('#protcolHandlersRadioBlock')
          .click();
      return browserProxy.whenCalled('setProtocolHandlerDefault');
    });
  });

  test('empty list', function() {
    return initPage().then(function() {
      const listFrames = testElement.root.querySelectorAll('.list-frame');
      assertEquals(0, listFrames.length);
    });
  });

  test('non-empty list', function() {
    browserProxy.setProtocolHandlers(protocols);

    return initPage().then(function() {
      const listFrames = testElement.root.querySelectorAll('.list-frame');
      const listItems = testElement.root.querySelectorAll('.list-item');
      // There are two protocols: ["mailto", "webcal"].
      assertEquals(2, listFrames.length);
      // There are three total handlers within the two protocols.
      assertEquals(3, listItems.length);

      // Check that item hosts are rendered correctly.
      const hosts = testElement.root.querySelectorAll('.protocol-host');
      assertEquals('www.google.com', hosts[0].textContent.trim());
      assertEquals('www.google1.com', hosts[1].textContent.trim());
      assertEquals('www.google2.com', hosts[2].textContent.trim());

      // Check that item default subtexts are rendered correctly.
      const defText = testElement.root.querySelectorAll('.protocol-default');
      assertFalse(defText[0].hidden);
      assertFalse(defText[1].hidden);
      assertTrue(defText[2].hidden);
    });
  });

  test('non-empty ignored protocols', () => {
    browserProxy.setIgnoredProtocols(ignoredProtocols);

    return initPage().then(() => {
      const listFrames = testElement.root.querySelectorAll('.list-frame');
      const listItems = testElement.root.querySelectorAll('.list-item');
      // There is a single blocked protocols section
      assertEquals(1, listFrames.length);
      // There is one total handlers within the two protocols.
      assertEquals(1, listItems.length);

      // Check that item hosts are rendered correctly.
      const hosts = testElement.root.querySelectorAll('.protocol-host');
      assertEquals('www.google.com', hosts[0].textContent.trim());

      // Check that item default subtexts are rendered correctly.
      const defText = testElement.root.querySelectorAll('.protocol-protocol');
      assertFalse(defText[0].hidden);
    });
  });

  /**
   * A reusable function to test different action buttons.
   * @param {string} button id of the button to test.
   * @param {string} handler name of browserProxy handler to react.
   * @return {!Promise}
   */
  function testButtonFlow(button, browserProxyHandler) {
    return initPage().then(() => {
      // Initiating the elements
      const menuButtons =
          testElement.root.querySelectorAll('cr-icon-button.icon-more-vert');
      assertEquals(3, menuButtons.length);
      const dialog = testElement.shadowRoot.querySelector('cr-action-menu');
      return Promise.all([[0, 0], [1, 0], [1, 1]].map((indices, menuIndex) => {
        const protocolIndex = indices[0];
        const handlerIndex = indices[1];
        // Test the button for the first protocol handler
        browserProxy.reset();
        assertFalse(dialog.open);
        menuButtons[menuIndex].click();
        assertTrue(dialog.open);
        if (testElement.$.defaultButton.disabled) {
          testElement.shadowRoot.querySelector('cr-action-menu').close();
          assertFalse(dialog.open);
        } else {
          testElement.$[button].click();
          assertFalse(dialog.open);
          return browserProxy.whenCalled(browserProxyHandler).then(args => {
            const protocol = args[0];
            const url = args[1];
            // BrowserProxy's handler is expected to be called with
            // arguments as [protocol, url].
            assertEquals(protocols[protocolIndex].protocol, protocol);
            assertEquals(
                protocols[protocolIndex].handlers[handlerIndex].spec, url);
          });
        }
      }));
    });
  }

  test('remove button works', function() {
    browserProxy.setProtocolHandlers(protocols);
    return testButtonFlow('removeButton', 'removeProtocolHandler');
  });

  test('default button works', function() {
    browserProxy.setProtocolHandlers(protocols);
    return testButtonFlow('defaultButton', 'setProtocolDefault').then(() => {
      const menuButtons =
          testElement.root.querySelectorAll('cr-icon-button.icon-more-vert');
      const closeMenu = () =>
          testElement.shadowRoot.querySelector('cr-action-menu').close();
      menuButtons[0].click();
      flush();
      assertTrue(testElement.$.defaultButton.disabled);
      closeMenu();
      menuButtons[1].click();
      flush();
      assertTrue(testElement.$.defaultButton.disabled);
      closeMenu();
      menuButtons[2].click();
      flush();
      assertFalse(testElement.$.defaultButton.disabled);
    });
  });

  test('remove button for ignored works', () => {
    browserProxy.setIgnoredProtocols(ignoredProtocols);
    return initPage()
        .then(() => {
          testElement.shadowRoot.querySelector('#removeIgnoredButton').click();
          return browserProxy.whenCalled('removeProtocolHandler');
        })
        .then(args => {
          const protocol = args[0];
          const url = args[1];
          // BrowserProxy's handler is expected to be called with arguments as
          // [protocol, url].
          assertEquals(ignoredProtocols[0].protocol, protocol);
          assertEquals(ignoredProtocols[0].spec, url);
        });
  });

  test('non-empty web app allowed protocols', async () => {
    browserProxy.setAppAllowedProtocolHandlers(appAllowedProtocols);
    await initPage();
    const listFrames = testElement.root.querySelectorAll('.list-frame');
    const listItems = testElement.root.querySelectorAll('.list-item');
    // There are two protocols: ["mailto", "webcal"].
    assertEquals(2, listFrames.length);
    // There are three total handlers within the two protocols.
    assertEquals(3, listItems.length);

    // Check that item hosts are rendered correctly.
    const hosts = testElement.root.querySelectorAll('.protocol-host');
    assertEquals('www.google.com', hosts[0].textContent.trim());
    assertEquals('www.google1.com', hosts[1].textContent.trim());
    assertEquals('www.google2.com', hosts[2].textContent.trim());
  });

  test('remove web app allowed protocols', async () => {
    browserProxy.setAppAllowedProtocolHandlers(appAllowedProtocols);
    await initPage();
    // Remove the first app protocol.
    testElement.shadowRoot.querySelector('#removeAppHandlerButton').click();
    const args = await browserProxy.whenCalled('removeAppAllowedHandler');

    // BrowserProxy's handler is expected to be called with
    // arguments as [protocol, url, app_id].
    assertEquals(appAllowedProtocols[0].protocol, args[0]);
    assertEquals(appAllowedProtocols[0].handlers[0].spec, args[1]);
    assertEquals(appAllowedProtocols[0].handlers[0].app_id, args[2]);
  });

  test('non-empty web app disallowed protocols', async () => {
    browserProxy.setAppDisallowedProtocolHandlers(appDisallowedProtocols);
    await initPage();
    const listFrames = testElement.root.querySelectorAll('.list-frame');
    const listItems = testElement.root.querySelectorAll('.list-item');
    // There are two protocols: ["mailto", "webcal"].
    assertEquals(2, listFrames.length);
    // There are three total handlers within the two protocols.
    assertEquals(3, listItems.length);

    // Check that item hosts are rendered correctly.
    const hosts = testElement.root.querySelectorAll('.protocol-host');
    assertEquals('www.google1.com', hosts[0].textContent.trim());
    assertEquals('www.google.com', hosts[1].textContent.trim());
    assertEquals('www.google3.com', hosts[2].textContent.trim());
  });

  test('remove web app disallowed protocols', async () => {
    browserProxy.setAppDisallowedProtocolHandlers(appDisallowedProtocols);
    await initPage();
    // Remove the first app protocol.
    testElement.shadowRoot.querySelector('#removeAppHandlerButton').click();
    const args = await browserProxy.whenCalled('removeAppDisallowedHandler');

    // BrowserProxy's handler is expected to be called with
    // arguments as [protocol, url, app_id].
    assertEquals(appDisallowedProtocols[0].protocol, args[0]);
    assertEquals(appDisallowedProtocols[0].handlers[0].spec, args[1]);
    assertEquals(appDisallowedProtocols[0].handlers[0].app_id, args[2]);
  });

  test('non-empty web app allowed and disallowed protocols', async () => {
    browserProxy.setAppAllowedProtocolHandlers(appAllowedProtocols);
    browserProxy.setAppDisallowedProtocolHandlers(appDisallowedProtocols);
    await initPage();
    const listFrames = testElement.root.querySelectorAll('.list-frame');
    const listItems = testElement.root.querySelectorAll('.list-item');
    // There are two protocols ["mailto", "webcal"] for both allowed,
    // and disallowed lists.
    assertEquals(4, listFrames.length);
    // There are three total handlers within the two protocols in both
    // the allowed and disallowed lists.
    assertEquals(6, listItems.length);

    // Check that item hosts are rendered correctly.
    const hosts = testElement.root.querySelectorAll('.protocol-host');

    // Allowed list.
    assertEquals('www.google.com', hosts[0].textContent.trim());
    assertEquals('www.google1.com', hosts[1].textContent.trim());
    assertEquals('www.google2.com', hosts[2].textContent.trim());

    // Disallowed list.
    assertEquals('www.google1.com', hosts[3].textContent.trim());
    assertEquals('www.google.com', hosts[4].textContent.trim());
    assertEquals('www.google3.com', hosts[5].textContent.trim());
  });

  test('remove web app allowed then disallowed protocols', async () => {
    browserProxy.setAppAllowedProtocolHandlers(appAllowedProtocols);
    browserProxy.setAppDisallowedProtocolHandlers(appDisallowedProtocols);
    await initPage();

    const removeButtons =
          testElement.root.querySelectorAll('cr-icon-button.icon-clear');
    assertEquals(6, removeButtons.length);

    // Remove the first allowed app protocol.
    removeButtons[0].click();
    const args1 = await browserProxy.whenCalled('removeAppAllowedHandler');
    assertEquals(appAllowedProtocols[0].protocol, args1[0]);
    assertEquals(appAllowedProtocols[0].handlers[0].spec, args1[1]);
    assertEquals(appAllowedProtocols[0].handlers[0].app_id, args1[2]);

    // Remove the first disallowed app protocol.
    removeButtons[3].click();
    const args2 = await browserProxy.whenCalled('removeAppDisallowedHandler');
    assertEquals(appDisallowedProtocols[0].protocol, args2[0]);
    assertEquals(appDisallowedProtocols[0].handlers[0].spec, args2[1]);
    assertEquals(appDisallowedProtocols[0].handlers[0].app_id, args2[2]);
  });
});
