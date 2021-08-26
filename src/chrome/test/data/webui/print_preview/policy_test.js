// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundGraphicsModeRestriction, DuplexMode, getInstance, NativeLayerImpl, PluginProxyImpl, PrintPreviewAppElement, PrintPreviewPluralStringProxyImpl} from 'chrome://print/print_preview.js';
// <if expr="chromeos or lacros">
import {ColorModeRestriction, DuplexModeRestriction, PinModeRestriction} from 'chrome://print/print_preview.js';
// </if>
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {TestPluginProxy} from 'chrome://test/print_preview/test_plugin_proxy.js';
import {TestPluralStringProxy} from 'chrome://test/test_plural_string_proxy.js';

// <if expr="chromeos or lacros">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

window.policy_tests = {};
policy_tests.suiteName = 'PolicyTest';
/** @enum {string} */
policy_tests.TestNames = {
  HeaderFooterPolicy: 'header/footer policy',
  CssBackgroundPolicy: 'css background policy',
  MediaSizePolicy: 'media size policy',
  SheetsPolicy: 'sheets policy',
  ColorPolicy: 'color policy',
  DuplexPolicy: 'duplex policy',
  PinPolicy: 'pin policy',
  PrintPdfAsImageAvailability: 'print as image available for PDF policy'
};

class PolicyTestPluralStringProxy extends TestPluralStringProxy {
  /** override */
  getPluralString(messageName, itemCount) {
    if (messageName === 'sheetsLimitErrorMessage') {
      this.methodCalled('getPluralString', {messageName, itemCount});
    }
    return Promise.resolve(this.text);
  }
}


suite(policy_tests.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /**
   * @param {!NativeInitialSettings} initialSettings
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function loadInitialSettings(initialSettings) {
    document.body.innerHTML = '';
    const nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(
        [{deviceName: initialSettings.printerName, printerName: 'FooName'}]);
    nativeLayer.setPageCount(3);
    NativeLayerImpl.instance_ = nativeLayer;
    // <if expr="chromeos or lacros">
    setNativeLayerCrosInstance();
    // </if>
    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.instance_ = pluginProxy;

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);

    // Wait for initialization to complete.
    return Promise
        .all([
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities')
        ])
        .then(function() {
          flush();
        });
  }

  /**
   * Sets up the Print Preview app, and loads initial settings with the given
   * policy.
   * @param {string} settingName Name of the setting to set up.
   * @param {string} serializedSettingName Name of the serialized setting.
   * @param {*} allowedMode Allowed value for the given setting.
   * @param {*} defaultMode Default value for the given setting.
   * @param {boolean} isPdf If settings are for previewing a PDF.
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function doAllowedDefaultModePolicySetup(
      settingName, serializedSettingName, allowedMode, defaultMode,
      isPdf = false) {
    const initialSettings = getDefaultInitialSettings(isPdf);

    if (allowedMode !== undefined || defaultMode !== undefined) {
      const policy = {};
      if (allowedMode !== undefined) {
        policy.allowedMode = allowedMode;
      }
      if (defaultMode !== undefined) {
        policy.defaultMode = defaultMode;
      }
      initialSettings.policies = {[settingName]: policy};
    }
    if (defaultMode !== undefined && serializedSettingName !== undefined) {
      // We want to make sure sticky settings get overridden.
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        [serializedSettingName]: !defaultMode,
      });
    }
    return loadInitialSettings(initialSettings);
  }

  /**
   * Sets up the Print Preview app, and loads initial settings with the
   * given policy.
   * @param {string} settingName Name of the setting to set up.
   * @param {string} serializedSettingName Name of the serialized setting.
   * @param {*} allowedMode Allowed value for the given setting.
   * @param {*} defaultMode Default value for the given setting.
   * @return {!Promise} A Promise that resolves once initial settings are
   *     done loading.
   */
  function doValuePolicySetup(settingName, value) {
    const initialSettings = getDefaultInitialSettings();
    if (value !== undefined) {
      const policy = {value: value};
      initialSettings.policies = {[settingName]: policy};
    }
    return loadInitialSettings(initialSettings);
  }

  function toggleMoreSettings() {
    const moreSettingsElement =
        page.shadowRoot.querySelector('print-preview-sidebar')
            .$$('print-preview-more-settings');
    moreSettingsElement.$.label.click();
  }

  function getCheckbox(settingName) {
    return page.shadowRoot.querySelector('print-preview-sidebar')
        .$$('print-preview-other-options-settings')
        .$$(`#${settingName}`);
  }

  // Tests different scenarios of applying header/footer policy.
  test(assert(policy_tests.TestNames.HeaderFooterPolicy), async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedDisabled: false,
        expectedChecked: true,
      },
      {
        // Restrict header/footer to be enabled.
        allowedMode: true,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedChecked: true,
      },
      {
        // Restrict header/footer to be disabled.
        allowedMode: false,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedChecked: false,
      },
      {
        // Check header/footer checkbox.
        allowedMode: undefined,
        defaultMode: true,
        expectedDisabled: false,
        expectedChecked: true,
      },
      {
        // Uncheck header/footer checkbox.
        allowedMode: undefined,
        defaultMode: false,
        expectedDisabled: false,
        expectedChecked: false,
      }
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'headerFooter', 'isHeaderFooterEnabled', subtestParams.allowedMode,
          subtestParams.defaultMode);
      toggleMoreSettings();
      const checkbox = getCheckbox('headerFooter');
      assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });

  // Tests different scenarios of applying background graphics policy.
  test(assert(policy_tests.TestNames.CssBackgroundPolicy), async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedDisabled: false,
        expectedChecked: false,
      },
      {
        // Restrict background graphics to be enabled.
        // Check that checkbox value default mode is not applied if it
        // contradicts allowed mode.
        allowedMode: BackgroundGraphicsModeRestriction.ENABLED,
        defaultMode: BackgroundGraphicsModeRestriction.DISABLED,
        expectedDisabled: true,
        expectedChecked: true,
      },
      {
        // Restrict background graphics to be disabled.
        allowedMode: BackgroundGraphicsModeRestriction.DISABLED,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedChecked: false,
      },
      {
        // Check background graphics checkbox.
        allowedMode: undefined,
        defaultMode: BackgroundGraphicsModeRestriction.ENABLED,
        expectedDisabled: false,
        expectedChecked: true,
      },
      {
        // Uncheck background graphics checkbox.
        allowedMode: BackgroundGraphicsModeRestriction.UNSET,
        defaultMode: BackgroundGraphicsModeRestriction.DISABLED,
        expectedDisabled: false,
        expectedChecked: false,
      }
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'cssBackground', 'isCssBackgroundEnabled', subtestParams.allowedMode,
          subtestParams.defaultMode);
      toggleMoreSettings();
      const checkbox = getCheckbox('cssBackground');
      assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
    }
  });

  // Tests different scenarios of applying default paper policy.
  test(assert(policy_tests.TestNames.MediaSizePolicy), async () => {
    const tests = [
      {
        // No policies.
        defaultMode: undefined,
        expectedName: 'NA_LETTER',
      },
      {
        // Not available option shouldn't change actual paper size setting.
        defaultMode: {width: 200000, height: 200000},
        expectedName: 'NA_LETTER',
      },
      {
        // Change default paper size setting.
        defaultMode: {width: 215900, height: 215900},
        expectedName: 'CUSTOM',
      }
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'mediaSize', /*serializedSettingName=*/ undefined,
          /*allowedMode=*/ undefined, subtestParams.defaultMode);
      toggleMoreSettings();
      const mediaSettingsSelect =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-media-size-settings')
              .$$('print-preview-settings-select')
              .$$('select');
      assertEquals(
          subtestParams.expectedName,
          JSON.parse(mediaSettingsSelect.value).name);
    }
  });

  test(assert(policy_tests.TestNames.SheetsPolicy), async () => {
    const pluralString = new PolicyTestPluralStringProxy();
    PrintPreviewPluralStringProxyImpl.setInstance(pluralString);
    pluralString.text = 'Exceeds limit of 1 sheet of paper';

    const tests = [
      {
        // No policy.
        maxSheets: 0,
        pages: [1, 2, 3],
        expectedDisabled: false,
        expectedHidden: true,
        expectedNonEmptyErrorMessage: false,
      },
      {
        // Policy is set, actual pages are not calculated yet.
        maxSheets: 3,
        pages: [],
        expectedDisabled: true,
        expectedHidden: true,
        expectedNonEmptyErrorMessage: false,
      },
      {
        // Policy is set, but the limit is not hit.
        maxSheets: 3,
        pages: [1, 2],
        expectedDisabled: false,
        expectedHidden: true,
        expectedNonEmptyErrorMessage: false,
      },
      {
        // Policy is set, the limit is hit, singular form is used.
        maxSheets: 1,
        pages: [1, 2],
        expectedDisabled: true,
        expectedHidden: false,
        expectedNonEmptyErrorMessage: true,
      },
      {
        // Policy is set, the limit is hit, plural form is used.
        maxSheets: 2,
        pages: [1, 2, 3, 4],
        expectedDisabled: true,
        expectedHidden: false,
        expectedNonEmptyErrorMessage: true,
      }
    ];
    for (const subtestParams of tests) {
      await doValuePolicySetup('sheets', subtestParams.maxSheets);
      pluralString.resetResolver('getPluralString');
      page.setSetting('pages', subtestParams.pages);
      if (subtestParams.expectedNonEmptyErrorMessage) {
        const {_, itemCount} = await pluralString.whenCalled('getPluralString');
        assertEquals(subtestParams.maxSheets, itemCount);
      }
      const printButton =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-button-strip')
              .shadowRoot.querySelector('cr-button.action-button');
      const errorMessage =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-button-strip')
              .shadowRoot.querySelector('div.error-message');
      assertEquals(subtestParams.expectedDisabled, printButton.disabled);
      assertEquals(subtestParams.expectedHidden, errorMessage.hidden);
      assertEquals(
          subtestParams.expectedNonEmptyErrorMessage,
          !errorMessage.hidden && !!errorMessage.innerText);
    }
  });

  // <if expr="chromeos or lacros">
  // Tests different scenarios of color printing policy.
  test(assert(policy_tests.TestNames.ColorPolicy), async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedDisabled: false,
        expectedValue: 'color',
      },
      {
        // Print in color by default.
        allowedMode: undefined,
        defaultMode: ColorModeRestriction.COLOR,
        expectedDisabled: false,
        expectedValue: 'color',
      },
      {
        // Print in black and white by default.
        allowedMode: undefined,
        defaultMode: ColorModeRestriction.MONOCHROME,
        expectedDisabled: false,
        expectedValue: 'bw',
      },
      {
        // Allowed and default policies unset.
        allowedMode: ColorModeRestriction.UNSET,
        defaultMode: ColorModeRestriction.UNSET,
        expectedDisabled: false,
        expectedValue: 'bw',
      },
      {
        // Allowed unset, default set to color printing.
        allowedMode: ColorModeRestriction.UNSET,
        defaultMode: ColorModeRestriction.COLOR,
        expectedDisabled: false,
        expectedValue: 'color',
      },
      {
        // Enforce color printing.
        allowedMode: ColorModeRestriction.COLOR,
        defaultMode: ColorModeRestriction.UNSET,
        expectedDisabled: true,
        expectedValue: 'color',
      },
      {
        // Enforce black and white printing.
        allowedMode: ColorModeRestriction.MONOCHROME,
        defaultMode: undefined,
        expectedDisabled: true,
        expectedValue: 'bw',
      },
      {
        // Enforce color printing, default is ignored.
        allowedMode: ColorModeRestriction.COLOR,
        defaultMode: ColorModeRestriction.MONOCHROME,
        expectedDisabled: true,
        expectedValue: 'color',
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'color', 'isColorEnabled', subtestParams.allowedMode,
          subtestParams.defaultMode);
      const colorSettingsSelect =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-color-settings')
              .shadowRoot.querySelector('select');
      assertEquals(
          subtestParams.expectedDisabled, colorSettingsSelect.disabled);
      assertEquals(subtestParams.expectedValue, colorSettingsSelect.value);
    }
  });

  // Tests different scenarios of duplex printing policy.
  test(assert(policy_tests.TestNames.DuplexPolicy), async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to SIMPLEX.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.SIMPLEX,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to UNSET.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.UNSET,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Allowed mode set to UNSET.
        allowedMode: DuplexModeRestriction.UNSET,
        defaultMode: undefined,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to LONG_EDGE.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.LONG_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to SHORT_EDGE.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.SHORT_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.SHORT_EDGE,
      },
      {
        // No restriction, default set to DUPLEX.
        allowedMode: undefined,
        defaultMode: DuplexModeRestriction.DUPLEX,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // No restriction, default set to SHORT_EDGE.
        allowedMode: DuplexModeRestriction.SIMPLEX,
        defaultMode: undefined,
        expectedChecked: false,
        expectedOpened: false,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to LONG_EDGE.
        allowedMode: DuplexModeRestriction.LONG_EDGE,
        defaultMode: undefined,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: true,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to SHORT_EDGE.
        allowedMode: DuplexModeRestriction.SHORT_EDGE,
        defaultMode: undefined,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: true,
        expectedValue: DuplexMode.SHORT_EDGE,
      },
      {
        // Restricted to DUPLEX.
        allowedMode: DuplexModeRestriction.DUPLEX,
        defaultMode: undefined,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: false,
        expectedValue: DuplexMode.LONG_EDGE,
      },
      {
        // Restricted to SHORT_EDGE, default is ignored.
        allowedMode: DuplexModeRestriction.SHORT_EDGE,
        defaultMode: DuplexModeRestriction.LONG_EDGE,
        expectedChecked: true,
        expectedOpened: true,
        expectedDisabled: true,
        expectedValue: DuplexMode.SHORT_EDGE,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'duplex', 'isDuplexEnabled', subtestParams.allowedMode,
          subtestParams.defaultMode);
      toggleMoreSettings();
      const duplexSettingsSection =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-duplex-settings');
      const checkbox = duplexSettingsSection.$$('cr-checkbox');
      const collapse = duplexSettingsSection.$$('iron-collapse');
      const select = duplexSettingsSection.$$('select');
      const expectedValue = subtestParams.expectedValue.toString();
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
      assertEquals(subtestParams.expectedOpened, collapse.opened);
      assertEquals(subtestParams.expectedDisabled, select.disabled);
      assertEquals(expectedValue, select.value);
    }
  });

  // Tests different scenarios of pin printing policy.
  test(assert(policy_tests.TestNames.PinPolicy), async () => {
    const tests = [
      {
        // No policies.
        allowedMode: undefined,
        defaultMode: undefined,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // No restriction, default set to UNSET.
        allowedMode: undefined,
        defaultMode: PinModeRestriction.UNSET,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // No restriction, default set to PIN.
        allowedMode: undefined,
        defaultMode: PinModeRestriction.PIN,
        expectedCheckboxDisabled: false,
        expectedChecked: true,
        expectedOpened: true,
        expectedInputDisabled: false,
      },
      {
        // No restriction, default set to NO_PIN.
        allowedMode: undefined,
        defaultMode: PinModeRestriction.NO_PIN,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // Restriction se to UNSET.
        allowedMode: PinModeRestriction.UNSET,
        defaultMode: undefined,
        expectedCheckboxDisabled: false,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // Restriction set to PIN.
        allowedMode: PinModeRestriction.PIN,
        defaultMode: undefined,
        expectedCheckboxDisabled: true,
        expectedChecked: true,
        expectedOpened: true,
        expectedInputDisabled: false,
      },
      {
        // Restriction set to NO_PIN.
        allowedMode: PinModeRestriction.NO_PIN,
        defaultMode: undefined,
        expectedCheckboxDisabled: true,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
      {
        // Restriction set to PIN, default is ignored.
        allowedMode: PinModeRestriction.NO_PIN,
        defaultMode: PinModeRestriction.PIN,
        expectedCheckboxDisabled: true,
        expectedChecked: false,
        expectedOpened: false,
        expectedInputDisabled: true,
      },
    ];
    for (const subtestParams of tests) {
      const initialSettings = getDefaultInitialSettings();

      if (subtestParams.allowedMode !== undefined ||
          subtestParams.defaultMode !== undefined) {
        const policy = {};
        if (subtestParams.allowedMode !== undefined) {
          policy.allowedMode = subtestParams.allowedMode;
        }
        if (subtestParams.defaultMode !== undefined) {
          policy.defaultMode = subtestParams.defaultMode;
        }
        initialSettings.policies = {"pin": policy};
      }

      const appState = {version: 2, "pinValue": "0000"};
      if (subtestParams.defaultMode !== undefined) {
        appState.isPinEnabled = !subtestParams.defaultMode;
      }
      initialSettings.serializedAppStateStr = JSON.stringify(appState);

      await loadInitialSettings(initialSettings);

      const pinSettingsSection =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-pin-settings');
      const checkbox = pinSettingsSection.$$('cr-checkbox');
      const collapse = pinSettingsSection.$$('iron-collapse');
      const input = pinSettingsSection.$$('cr-input');
      assertEquals(subtestParams.expectedCheckboxDisabled, checkbox.disabled);
      assertEquals(subtestParams.expectedChecked, checkbox.checked);
      assertEquals(subtestParams.expectedOpened, collapse.opened);
      assertEquals(subtestParams.expectedInputDisabled, input.disabled);
    }
  });
  // </if>

  // <if expr="is_win or is_macosx">
  // Tests different scenarios of PDF print as image option policy.
  // The otherOptions section will be visible for non-PDF cases, and only for
  // PDF when the policy explicitly allows print as image.
  test(assert(policy_tests.TestNames.PrintPdfAsImageAvailability), async () => {
    const tests = [
      {
        // No policies with modifiable content.
        allowedMode: undefined,
        isPdf: false,
        otherOptionsHidden: false,
        expectedAvailable: false,
      },
      {
        // No policies with PDF content.
        allowedMode: undefined,
        isPdf: true,
        otherOptionsHidden: true,
        expectedAvailable: false,
      },
      {
        // Explicitly restrict "Print as image" option for modifiable content.
        allowedMode: false,
        isPdf: false,
        otherOptionsHidden: false,
        expectedAvailable: false,
      },
      {
        // Explicitly restrict "Print as image" option for PDF content.
        allowedMode: false,
        isPdf: true,
        otherOptionsHidden: true,
        expectedAvailable: false,
      },
      {
        // Explicitly enable "Print as image" option for modifiable content.
        allowedMode: true,
        isPdf: false,
        otherOptionsHidden: false,
        expectedAvailable: false,
      },
      {
        // Explicitly enable "Print as image" option for PDF content.
        allowedMode: true,
        isPdf: true,
        otherOptionsHidden: false,
        expectedAvailable: true,
      },
    ];
    for (const subtestParams of tests) {
      await doAllowedDefaultModePolicySetup(
          'printPdfAsImageAvailability', 'isRasterizeEnabled',
          subtestParams.allowedMode,
          /*defaultMode=*/ undefined, /*isPdf=*/ subtestParams.isPdf);
      toggleMoreSettings();
      const otherSettingsSection =
          page.shadowRoot.querySelector('print-preview-sidebar')
              .$$('print-preview-other-options-settings');
      const rasterize = getInstance().getSetting('rasterize');
      expectEquals(
          subtestParams.otherOptionsHidden, otherSettingsSection.hidden);
      expectEquals(subtestParams.expectedAvailable, rasterize.available);
    }
  });
  // </if>
});
