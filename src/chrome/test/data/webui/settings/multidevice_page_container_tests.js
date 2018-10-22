// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {settings.MultideviceBrowserProxy}
 * Note: showMultiDeviceSetupDialog is not used by the
 * multidevice-page-container element.
 */
class TestMultideviceBrowserProxy extends TestBrowserProxy {
  constructor(initialPageContentData) {
    super([
      'showMultiDeviceSetupDialog',
      'getPageContentData',
    ]);
    this.data = initialPageContentData;
  }

  /** @override */
  getPageContentData() {
    this.methodCalled('getPageContentData');
    return Promise.resolve(this.data);
  }
}

suite('Multidevice', function() {
  let multidevicePageContainer = null;
  let browserProxy = null;
  let ALL_MODES;

  /**
   * @param {!settings.MultiDeviceSettingsMode} mode
   * @param {boolean} isSuiteSupported
   * @return {!MultiDevicePageContentData}
   */
  function getFakePageContentData(mode, isSuiteSupported) {
    return {
      mode: mode,
      hostDeviceName: [
        settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
        settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
        settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
      ].includes(mode) ?
          'Pixel XL' :
          undefined,
      betterTogetherState: isSuiteSupported ?
          settings.MultiDeviceFeatureState.ENABLED_BY_USER :
          settings.MultiDeviceFeatureState.NOT_SUPPORTED_BY_CHROMEBOOK,
    };
  }

  /**
   * @param {!settings.MultiDeviceSettingsMode} newMode
   * @param {boolean} isSuiteSupported
   */
  function changePageContent(newMode, isSuiteSupported) {
    cr.webUIListenerCallback(
        'settings.updateMultidevicePageContentData',
        getFakePageContentData(newMode, isSuiteSupported));
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    ALL_MODES = Object.values(settings.MultiDeviceSettingsMode);
  });

  /** @return {?HTMLElement} */
  const getMultidevicePage = () =>
      multidevicePageContainer.$$('settings-multidevice-page');

  // Initializes page container with provided mode and sets the Better Together
  // suite's feature state based on provided boolean. It returns a promise that
  // only resolves when the multidevice-page has changed its pageContentData and
  // the ensuing callbacks back been flushed.
  /**
   * @param {!settings.MultiDeviceSettingsMode} mode
   * @param {boolean} isSuiteSupported
   * @return {!Promise}
   */
  function setInitialPageContent(mode, isSuiteSupported) {
    if (multidevicePageContainer)
      multidevicePageContainer.remove();
    browserProxy = new TestMultideviceBrowserProxy(
        getFakePageContentData(mode, isSuiteSupported));
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    multidevicePageContainer =
        document.createElement('settings-multidevice-page-container');
    document.body.appendChild(multidevicePageContainer);

    return browserProxy.whenCalled('getPageContentData')
        .then(Polymer.dom.flush());
  }

  test(
      'WebUIListener toggles multidevice page based suite support in all modes',
      function() {
        return setInitialPageContent(
                   settings.MultiDeviceSettingsMode.NO_HOST_SET,
                   /* isSuiteSupported */ true)
            .then(() => {
              for (const mode of ALL_MODES) {
                // Check that the settings-page is visible iff the suite is
                // supported.
                changePageContent(mode, /* isSuiteSupported */ true);
                assertTrue(!!getMultidevicePage());

                changePageContent(mode, /* isSuiteSupported */ false);
                assertFalse(!!getMultidevicePage());

                changePageContent(mode, /* isSuiteSupported */ true);
                assertTrue(!!getMultidevicePage());
              }
            });
      });

  test(
      'multidevice-page is not attached if suite is not supported', function() {
        return setInitialPageContent(
                   settings.MultiDeviceSettingsMode.NO_HOST_SET,
                   /* isSuiteSupported */ false)
            .then(() => assertFalse(!!getMultidevicePage()));
      });
});
