// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://test/cr_elements/cr_policy_strings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ChooserException, ChooserExceptionListEntryElement, ChooserType, ContentSetting,ContentSettingsTypes, SiteException, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
// clang-format on

/**
 * @fileoverview Suite of tests for chooser-exception-list-entry.
 */

suite('ChooserExceptionListEntry', function() {
  /**
   * A chooser exception list entry element created before each test.
   */
  let testElement: ChooserExceptionListEntryElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a chooser-exception-list-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    testElement = document.createElement('chooser-exception-list-entry');
    document.body.appendChild(testElement);
  });

  function createSiteException(
      origin: string, override?: Partial<SiteException>): SiteException {
    return Object.assign(
        {
          category: ContentSettingsTypes.USB_DEVICES,
          embeddingOrigin: origin,
          incognito: false,
          origin: origin,
          displayName: origin,
          setting: ContentSetting.DEFAULT,
          settingDetail: null,
          enforcement: null,
          controlledBy: chrome.settingsPrivate.ControlledBy.PRIMARY_USER,
          isEmbargoed: false,
        },
        override || {});
  }

  function createChooserException(
      chooserType: ChooserType, sites: SiteException[],
      override?: Partial<ChooserException>): ChooserException {
    return Object.assign(
        {
          chooserType: chooserType,
          displayName: '',
          object: {},
          sites: sites,
        },
        override || {});
  }

  test(
      'User granted chooser exceptions should show the reset button',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button container is hidden.
        const dotsMenu = siteListEntry!.$.actionMenuButton;
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is not hidden.
        const resetButton = siteListEntry!.$.resetSite;
        assertFalse(resetButton.hidden);

        // Ensure that the policy enforced indicator is hidden.
        const policyIndicator = siteListEntry!.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        assertFalse(!!policyIndicator);
      });

  test(
      'Policy granted chooser exceptions should show the policy indicator ' +
          'icon',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button container is hidden.
        const dotsMenu = siteListEntry!.$.actionMenuButton;
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is hidden.
        const resetButton = siteListEntry!.$.resetSite;
        assertTrue(resetButton.hidden);

        // Ensure that the policy enforced indicator is not hidden.
        const policyIndicator = siteListEntry!.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);
      });

  test(
      'Site exceptions from mixed sources should display properly', function() {
        // The SiteExceptions returned by the getChooserExceptionList will be
        // ordered by provider source, then alphabetically by requesting origin
        // and embedding origin.
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
              createSiteException('https://bar.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntries =
            testElement.$.listContainer.querySelectorAll('site-list-entry');
        assertTrue(!!siteListEntries);
        assertEquals(siteListEntries.length, 2);

        // The first entry should be policy enforced.
        const firstDotsMenu = siteListEntries[0]!.$.actionMenuButton;
        assertTrue(!!firstDotsMenu);
        assertTrue(firstDotsMenu!.hidden);

        const firstResetButton = siteListEntries[0]!.$.resetSite;
        assertTrue(firstResetButton!.hidden);

        const firstPolicyIndicator =
            siteListEntries[0]!.shadowRoot!.querySelector(
                'cr-policy-pref-indicator');
        assertTrue(!!firstPolicyIndicator);

        // The second entry should be user granted.
        const secondDotsMenu = siteListEntries[1]!.$.actionMenuButton;
        assertTrue(!!secondDotsMenu);
        assertTrue(secondDotsMenu!.hidden);

        const secondResetButton = siteListEntries[1]!.$.resetSite;
        assertFalse(secondResetButton!.hidden);

        const secondPolicyIndicator =
            siteListEntries[1]!.shadowRoot!.querySelector(
                'cr-policy-pref-indicator');
        assertFalse(!!secondPolicyIndicator);
      });

  test(
      'The show-tooltip event is fired when mouse hovers over policy indicator',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        const policyIndicator = siteListEntry!.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);

        const icon =
            policyIndicator!.shadowRoot!.querySelector('cr-tooltip-icon');
        assertTrue(!!icon);

        const paperTooltip = icon!.shadowRoot!.querySelector('paper-tooltip');
        assertTrue(!!paperTooltip);

        // This tooltip is never shown since a common tooltip will be used.
        assertTrue(!!paperTooltip);
        assertEquals(
            'none',
            (paperTooltip!.computedStyleMap().get('display') as
             {value: string})!.value);
        assertFalse(paperTooltip!._showing);

        const wait = eventToPromise('show-tooltip', document);
        icon!.$.indicator.dispatchEvent(
            new MouseEvent('mouseenter', {bubbles: true, composed: true}));
        return wait.then(() => {
          assertTrue(paperTooltip!._showing);
          assertEquals(
              'none',
              (paperTooltip!.computedStyleMap().get('display') as
               {value: string})!.value);
        });
      });

  test(
      'The reset button calls the resetChooserExceptionForSite method',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button is hidden.
        const dotsMenu = siteListEntry!.$.actionMenuButton;
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is not hidden.
        const resetButton = siteListEntry!.$.resetSite;
        assertFalse(resetButton.hidden);

        resetButton!.click();
        return browserProxy.whenCalled('resetChooserExceptionForSite')
            .then(function(args) {
              // The args should be the chooserType, origin, embeddingOrigin,
              // and object.
              assertEquals(ChooserType.USB_DEVICES, args[0]);
              assertEquals('https://foo.com', args[1]);
              assertEquals('https://foo.com', args[2]);
              assertEquals('object', typeof args[3]);
            });
      });
});
