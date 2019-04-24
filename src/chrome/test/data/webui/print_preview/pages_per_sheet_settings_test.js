// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('pages_per_sheet_settings_test', function() {
  suite('PagesPerSheetSettingsTest', function() {
    let pagesPerSheetSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      pagesPerSheetSection =
          document.createElement('print-preview-pages-per-sheet-settings');
      pagesPerSheetSection.settings = {
        pagesPerSheet: {
          value: 1,
          unavailableValue: 1,
          valid: true,
          available: true,
          setByPolicy: false,
          key: '',
        },
      };
      pagesPerSheetSection.disabled = false;
      document.body.appendChild(pagesPerSheetSection);
    });

    // Tests that setting the setting updates the UI.
    test('set setting', async () => {
      const select = pagesPerSheetSection.$$('select');
      assertEquals('1', select.value);

      pagesPerSheetSection.set('settings.pagesPerSheet.value', 4);
      await test_util.eventToPromise(
          'process-select-change', pagesPerSheetSection);
      assertEquals('4', select.value);
    });

    // Tests that selecting a new option in the dropdown updates the setting.
    test('select option', async () => {
      // Verify that the selected option and names are as expected.
      const select = pagesPerSheetSection.$$('select');
      assertEquals('1', select.value);
      assertEquals(1, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
      assertEquals(6, select.options.length);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(pagesPerSheetSection, '2');
      assertEquals(2, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    });
  });
});
