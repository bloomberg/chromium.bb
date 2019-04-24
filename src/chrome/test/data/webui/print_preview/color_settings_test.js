// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('color_settings_test', function() {
  suite('ColorSettingsTest', function() {
    let colorSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      colorSection = document.createElement('print-preview-color-settings');
      colorSection.settings = {
        color: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isColorEnabled',
        },
      };
      colorSection.disabled = false;
      document.body.appendChild(colorSection);
    });

    // Tests that setting the setting updates the UI.
    test('set setting', async () => {
      const select = colorSection.$$('select');
      assertEquals('color', select.value);

      colorSection.set('settings.color.value', false);
      await test_util.eventToPromise('process-select-change', colorSection);
      assertEquals('bw', select.value);
    });

    // Tests that selecting a new option in the dropdown updates the setting.
    test('select option', async () => {
      // Verify that the selected option and names are as expected.
      const select = colorSection.$$('select');
      assertEquals('color', select.value);
      assertTrue(colorSection.getSettingValue('color'));
      assertEquals(2, select.options.length);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(colorSection, 'bw');
      assertFalse(colorSection.getSettingValue('color'));
    });

    if (cr.isChromeOS) {
      // Tests that if the setting is enforced by enterprise policy it is
      // disabled.
      test('disabled by policy', function() {
        // Verify that the selected option and names are as expected.
        const select = colorSection.$$('select');
        assertFalse(select.disabled);

        colorSection.set('settings.color.setByPolicy', true);
        assertTrue(select.disabled);
      });
    }
  });
});
