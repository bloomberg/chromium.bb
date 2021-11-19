// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isLacros, isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SafeBrowsingSetting, SettingsSecurityPageElement} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, PrivacyPageBrowserProxyImpl, Router, routes, SafeBrowsingInteractions, SecureDnsMode} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

suite('CrSettingsSecurityPageTest', function() {
  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {!TestPrivacyPageBrowserProxy} */
  let testPrivacyBrowserProxy;

  /** @type {!SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: true,
      showHttpsOnlyModeSetting: true,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    document.body.innerHTML = '';
    page = /** @type {!SettingsSecurityPageElement} */ (
        document.createElement('settings-security-page'));
    page.prefs = {
      profile: {password_manager_leak_detection: {value: false}},
      safebrowsing: {
        scout_reporting_enabled: {value: true},
      },
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
        password_leak_detection: {value: false},
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
      https_only_mode_enabled: {value: false},
    };
    document.body.appendChild(page);
    page.shadowRoot.querySelector('#safeBrowsingEnhanced').updateCollapsed();
    page.shadowRoot.querySelector('#safeBrowsingStandard').updateCollapsed();
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  if (isMac || isWindows) {
    test('NativeCertificateManager', function() {
      page.shadowRoot.querySelector('#manageCertificates').click();
      return testPrivacyBrowserProxy.whenCalled('showManageSSLCertificates');
    });
  }

  // Initially specified pref option should be expanded
  test('SafeBrowsingRadio_InitialPrefOptionIsExpanded', function() {
    assertFalse(
        page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
  });

  // TODO(crbug.com/1148302): This class directly calls
  // `GetNSSCertDatabaseForProfile()` that causes crash at the moment and is
  // never called from Lacros-Chrome. This should be revisited when there is a
  // solution for the client certificates settings page on Lacros-Chrome.
  if (!isLacros) {
    test('LogManageCerfificatesClick', async function() {
      page.shadowRoot.querySelector('#manageCertificates').click();
      const result = await testMetricsBrowserProxy.whenCalled(
          'recordSettingsPageHistogram');
      assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);
    });
  }

  test('ManageSecurityKeysSubpageVisible', function() {
    assertTrue(isChildVisible(page, '#security-keys-subpage-trigger'));
  });

  test('PasswordsLeakDetectionSubLabel', function() {
    const toggle = page.shadowRoot.querySelector('#passwordsLeakToggle');
    const defaultSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription');
    const activeWhenSignedInSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription') +
        ' ' +
        loadTimeData.getString(
            'passwordsLeakDetectionSignedOutEnabledDescription');
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', true);
    page.set(
        'prefs.generated.password_leak_detection.userControlDisabled', true);
    flush();
    assertEquals(activeWhenSignedInSubLabel, toggle.subLabel);

    page.set('prefs.generated.password_leak_detection.value', true);
    page.set(
        'prefs.generated.password_leak_detection.userControlDisabled', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);
  });

  test('LogSafeBrowsingExtendedToggle', async function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();

    page.shadowRoot.querySelector('#safeBrowsingReportingToggle').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IMPROVE_SECURITY, result);
  });

  test('safeBrowsingReportingToggle', function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    const safeBrowsingReportingToggle =
        page.shadowRoot.querySelector('#safeBrowsingReportingToggle');
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);

    // This could also be set to disabled, anything other than standard.
    page.shadowRoot.querySelector('#safeBrowsingEnhanced').click();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);

    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnRepeatSelection',
      function() {
        page.shadowRoot.querySelector('#safeBrowsingStandard').click();
        flush();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
        assertFalse(
            page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);

        // Expanding another radio button should not collapse already expanded
        // option.
        page.shadowRoot.querySelector('#safeBrowsingEnhanced')
            .shadowRoot.querySelector('cr-expand-button')
            .click();
        flush();
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);

        // Clicking on already selected button should not collapse manually
        // expanded option.
        page.shadowRoot.querySelector('#safeBrowsingStandard').click();
        flush();
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);
      });

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnSelectedChanged',
      async function() {
        page.shadowRoot.querySelector('#safeBrowsingStandard').click();
        flush();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);

        page.shadowRoot.querySelector('#safeBrowsingEnhanced')
            .shadowRoot.querySelector('cr-expand-button')
            .click();
        flush();
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);

        page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
        flush();

        // Previously selected option must remain opened.
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);

        page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
            .shadowRoot.querySelector('.action-button')
            .click();
        flush();

        // Wait for onDisableSafebrowsingDialogClose_ to finish.
        await flushTasks();

        // The deselected option should become collapsed.
        assertFalse(
            page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
        assertTrue(
            page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);
      });

  test('DisableSafebrowsingDialog_Confirm', async function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();

    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);

    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(
        null,
        page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog'));

    assertFalse(page.shadowRoot.querySelector('#safeBrowsingEnhanced').checked);
    assertFalse(page.shadowRoot.querySelector('#safeBrowsingStandard').checked);
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.DISABLED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromEnhanced', async function() {
    page.shadowRoot.querySelector('#safeBrowsingEnhanced').click();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();

    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);

    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.cancel-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(
        null,
        page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog'));

    assertTrue(page.shadowRoot.querySelector('#safeBrowsingEnhanced').checked);
    assertFalse(page.shadowRoot.querySelector('#safeBrowsingStandard').checked);
    assertFalse(page.shadowRoot.querySelector('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromStandard', async function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();

    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);

    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.cancel-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(
        null,
        page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog'));

    assertFalse(page.shadowRoot.querySelector('#safeBrowsingEnhanced').checked);
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').checked);
    assertFalse(page.shadowRoot.querySelector('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
  });

  test('noControlSafeBrowsingReportingInEnhanced', function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();

    assertFalse(
        page.shadowRoot.querySelector('#safeBrowsingReportingToggle').disabled);
    page.shadowRoot.querySelector('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.shadowRoot.querySelector('#safeBrowsingReportingToggle').disabled);
  });

  test('noValueChangeSafeBrowsingReportingInEnhanced', function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.shadowRoot.querySelector('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noControlSafeBrowsingReportingInDisabled', async function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();

    assertFalse(
        page.shadowRoot.querySelector('#safeBrowsingReportingToggle').disabled);
    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);

    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.shadowRoot.querySelector('#safeBrowsingReportingToggle').disabled);
  });

  test('noValueChangeSafeBrowsingReportingInDisabled', async function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);

    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noValueChangePasswordLeakSwitchToEnhanced', function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.shadowRoot.querySelector('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('noValuePasswordLeakSwitchToDisabled', async function() {
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);

    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('safeBrowsingUserActionRecorded', async function() {
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    page.shadowRoot.querySelector('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    // Not logged because it is already in standard mode.
    assertEquals(
        0,
        testMetricsBrowserProxy.getCallCount(
            'recordSafeBrowsingInteractionHistogram'));

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot.querySelector('#safeBrowsingEnhanced').click();
    flush();
    const [enhancedClickedResult, enhancedClickedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED,
        enhancedClickedResult);
    assertEquals(
        'SafeBrowsing.Settings.EnhancedProtectionClicked',
        enhancedClickedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot.querySelector('#safeBrowsingEnhanced')
        .shadowRoot.querySelector('cr-expand-button')
        .click();
    flush();
    const [enhancedExpandedResult, enhancedExpandedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED,
        enhancedExpandedResult);
    assertEquals(
        'SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked',
        enhancedExpandedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot.querySelector('#safeBrowsingStandard')
        .shadowRoot.querySelector('cr-expand-button')
        .click();
    flush();
    const [standardExpandedResult, standardExpandedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED,
        standardExpandedResult);
    assertEquals(
        'SafeBrowsing.Settings.StandardProtectionExpandArrowClicked',
        standardExpandedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();
    const [disableClickedResult, disableClickedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED,
        disableClickedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingClicked',
        disableClickedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.cancel-button')
        .click();
    flush();
    const [disableDeniedResult, disableDeniedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED,
        disableDeniedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied',
        disableDeniedAction);

    await flushTasks();

    page.shadowRoot.querySelector('#safeBrowsingDisabled').click();
    flush();
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot.querySelector('settings-disable-safebrowsing-dialog')
        .shadowRoot.querySelector('.action-button')
        .click();
    flush();
    const [disableConfirmedResult, disableConfirmedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED,
        disableConfirmedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingDialogConfirmed',
        disableConfirmedAction);
  });

  test('securityPageShowedRecorded', async function() {
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ null,
        /* removeSearch= */ true);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_SHOWED,
        await testMetricsBrowserProxy.whenCalled(
            'recordSafeBrowsingInteractionHistogram'));
  });

  test('enhancedProtectionAutoExpanded', function() {
    // Standard protection should be pre-expanded if there is no param.
    Router.getInstance().navigateTo(routes.SECURITY);
    assertFalse(
        page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
    // Enhanced protection should be pre-expanded if the param is set to
    // enhanced.
    Router.getInstance().navigateTo(
        routes.SECURITY,
        /* dynamicParams= */ new URLSearchParams('q=enhanced'));
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    assertTrue(page.shadowRoot.querySelector('#safeBrowsingEnhanced').expanded);
    assertFalse(
        page.shadowRoot.querySelector('#safeBrowsingStandard').expanded);
  });

  // Tests that toggling the HTTPS-Only Mode setting sets the associated pref.
  test('httpsOnlyModeToggle', function() {
    const httpsOnlyModeToggle =
        page.shadowRoot.querySelector('#httpsOnlyModeToggle');
    assertFalse(page.prefs.https_only_mode_enabled.value);
    httpsOnlyModeToggle.click();
    assertTrue(page.prefs.https_only_mode_enabled.value);
  });
});


suite('CrSettingsSecurityPageTest_FlagsDisabled', function() {
  /** @type {!SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: false,
      showHttpsOnlyModeSetting: false,
    });
  });

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsSecurityPageElement} */ (
        document.createElement('settings-security-page'));
    page.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing: {
        scout_reporting_enabled: {value: true},
      },
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
        password_leak_detection: {value: true, userControlDisabled: false},
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('ManageSecurityKeysSubpageHidden', function() {
    assertFalse(isChildVisible(page, '#security-keys-subpage-trigger'));
  });

  test('HttpsOnlyModeSettingHidden', function() {
    assertFalse(isChildVisible(page, '#httpsOnlyModeToggle'));
  });
});
