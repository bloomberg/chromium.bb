// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrivacyPageBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestPrivacyPageBrowserProxy} from 'chrome://test/settings/test_privacy_page_browser_proxy.js';
import {flushTasks} from 'chrome://test/test_util.js';

// clang-format on

suite('metrics reporting', function() {
  /** @type {TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsPrivacyPageElement} */
  let page;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    PolymerTest.clearBody();
    page = document.createElement('settings-personalization-options');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('changes to whether metrics reporting is enabled/managed', function() {
    let toggled;
    return testBrowserProxy.whenCalled('getMetricsReporting')
        .then(function() {
          return flushTasks();
        })
        .then(function() {
          const control = page.$.metricsReportingControl;
          assertEquals(
              testBrowserProxy.metricsReporting.enabled, control.checked);
          assertEquals(
              testBrowserProxy.metricsReporting.managed,
              !!control.pref.controlledBy);

          const changedMetrics = {
            enabled: !testBrowserProxy.metricsReporting.enabled,
            managed: !testBrowserProxy.metricsReporting.managed,
          };
          webUIListenerCallback('metrics-reporting-change', changedMetrics);
          flush();

          assertEquals(changedMetrics.enabled, control.checked);
          assertEquals(changedMetrics.managed, !!control.pref.controlledBy);

          toggled = !changedMetrics.enabled;
          control.checked = toggled;
          control.notifyChangedByUserInteraction();

          return testBrowserProxy.whenCalled('setMetricsReportingEnabled');
        })
        .then(function(enabled) {
          assertEquals(toggled, enabled);
        });
  });

  test('metrics reporting restart button', function() {
    return testBrowserProxy.whenCalled('getMetricsReporting').then(function() {
      flush();

      // Restart button should be hidden by default (in any state).
      assertFalse(!!page.shadowRoot.querySelector('#restart'));

      // Simulate toggling via policy.
      webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: true,
      });

      // No restart button should show because the value is managed.
      assertFalse(!!page.shadowRoot.querySelector('#restart'));

      webUIListenerCallback('metrics-reporting-change', {
        enabled: true,
        managed: true,
      });
      flush();

      // Changes in policy should not show the restart button because the value
      // is still managed.
      assertFalse(!!page.shadowRoot.querySelector('#restart'));

      // Remove the policy and toggle the value.
      webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: false,
      });
      flush();

      // Now the restart button should be showing.
      assertTrue(!!page.shadowRoot.querySelector('#restart'));

      // Receiving the same values should have no effect.
      webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: false,
      });
      flush();
      assertTrue(!!page.shadowRoot.querySelector('#restart'));
    });
  });
});
