// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_sidebar_test', function() {
  /** @enum {string} */
  const TestNames = {
    SettingsSectionsVisibilityChange: 'settings sections visibility change',
  };

  const suiteName = 'PrintPreviewSidebarTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewSidebarElement} */
    let sidebar = null;

    /** @type {?PrintPreviewModelElement} */
    let model = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?cloudprint.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @override */
    setup(function() {
      // Stub out the native layer and cloud print interface
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate('FooDevice'));
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();

      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      sidebar = document.createElement('print-preview-sidebar');
      sidebar.settings = model.settings;
      test_util.fakeDataBind(model, sidebar, 'settings');
      document.body.appendChild(sidebar);
      sidebar.init(false, 'FooDevice', null);
      sidebar.cloudPrintInterface = cloudPrintInterface;

      return nativeLayer.whenCalled('getPrinterCapabilities');
    });

    test(assert(TestNames.SettingsSectionsVisibilityChange), function() {
      const moreSettingsElement = sidebar.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
      const camelToKebab = s => s.replace(/([A-Z])/g, '-$1').toLowerCase();
      ['copies', 'layout', 'color', 'mediaSize', 'margins', 'dpi', 'scaling',
       'duplex', 'otherOptions']
          .forEach(setting => {
            const element =
                sidebar.$$(`print-preview-${camelToKebab(setting)}-settings`);
            // Show, hide and reset.
            [true, false, true].forEach(value => {
              sidebar.set(`settings.${setting}.available`, value);
              // Element expected to be visible when available.
              assertEquals(!value, element.hidden);
            });
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
